/*
 * Copyright (c) 2017 Ambroz Bizjak
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <cstring>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <cstddef>
#include <stdexcept>
#include <utility>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <linux/if_tun.h>

#include <uv.h>

#include <aipstack/misc/Assert.h>
#include <aipstack/proto/EthernetProto.h>

#include "tap_linux.h"

namespace AIpStackExamples {

inline static bool ErrIsEAGAINorEWOULDBLOCK (int err)
{
    if (err == EAGAIN) {
        return true;
    }
#if EWOULDBLOCK != EAGAIN
    if (err == EWOULDBLOCK) {
        return true;
    }
#endif
    return false;
}

TapDevice::TapDevice (uv_loop_t *loop, std::string const &device_id) :
    m_loop(loop),
    m_active(true)
{
    FileDescriptorWrapper fd{::open("/dev/net/tun", O_RDWR)};
    if (fd.get() < 0) {
        throw std::runtime_error("Failed to open /dev/net/tun.");
    }
    
    fd.setNonblocking();
    
    struct ifreq ifr;
    
    std::memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags |= IFF_NO_PI|IFF_TAP;
    std::snprintf(ifr.ifr_name, IFNAMSIZ, "%s", device_id.c_str());
    
    if (::ioctl(fd.get(), TUNSETIFF, (void *)&ifr) < 0) {
        throw std::runtime_error("ioctl(TUNSETIFF) failed.");
    }
    
    std::string devname_real(ifr.ifr_name);
    
    FileDescriptorWrapper sock{::socket(AF_INET, SOCK_DGRAM, 0)};
    if (sock.get() < 0) {
        throw std::runtime_error("socket(AF_INET, SOCK_DGRAM) failed.");
    }
    
    std::memset(&ifr, 0, sizeof(ifr));
    std::strcpy(ifr.ifr_name, devname_real.c_str());
    
    if (::ioctl(sock.get(), SIOCGIFMTU, (void *)&ifr) < 0) {
        throw std::runtime_error("ioctl(SIOCGIFMTU) failed.");
    }
    
    m_frame_mtu = ifr.ifr_mtu + AIpStack::EthHeader::Size;
    
    m_read_buffer.resize(m_frame_mtu);
    m_write_buffer.resize(m_frame_mtu);
    
    if (m_poll.initialize([&](uv_poll_t *dst) {
        return uv_poll_init(m_loop, dst, fd.get());
    }) != 0) {
        throw std::runtime_error("uv_poll_init failed.");
    }
    
    m_poll.get()->data = this;
    m_poll.user().fd = std::move(fd);
    
    if (uv_poll_start(m_poll.get(), UV_READABLE, &TapDevice::pollCbTrampoline) != 0) {
        throw std::runtime_error("uv_poll_start failed.");
    }
}

TapDevice::~TapDevice ()
{
}

std::size_t TapDevice::getMtu () const
{
    return m_frame_mtu;
}

AIpStack::IpErr TapDevice::sendFrame (AIpStack::IpBufRef frame)
{
    if (!m_active) {
        return AIpStack::IpErr::HW_ERROR;
    }
    
    if (frame.tot_len < AIpStack::EthHeader::Size) {
        return AIpStack::IpErr::HW_ERROR;
    }
    else if (frame.tot_len > m_frame_mtu) {
        return AIpStack::IpErr::PKT_TOO_LARGE;
    }
    
    char *buffer = m_write_buffer.data();
    
    std::size_t len = frame.tot_len;
    frame.takeBytes(len, buffer);
    
    auto write_res = ::write(m_poll.user().fd.get(), buffer, len);
    if (write_res < 0) {
        int error = errno;
        if (ErrIsEAGAINorEWOULDBLOCK(error)) {
            return AIpStack::IpErr::BUFFER_FULL;
        }
        return AIpStack::IpErr::HW_ERROR;
    }
    if ((std::size_t)write_res != len) {
        return AIpStack::IpErr::HW_ERROR;
    }
    
    return AIpStack::IpErr::SUCCESS;
}

void TapDevice::pollCbTrampoline (uv_poll_t *handle, int status, int events)
{
    TapDevice *obj = reinterpret_cast<TapDevice *>(handle->data);
    AIPSTACK_ASSERT(handle == obj->m_poll.get())
    
    obj->pollCb(status, events);
}

void TapDevice::pollCb (int status, int events)
{
    AIPSTACK_ASSERT(m_active)
    
    do {
        if (status < 0) {
            std::fprintf(stderr, "TapDevice: poll error, status=%d. Stopping.\n",
                         status);
            goto error;
        }
        
        if ((events & UV_DISCONNECT) != 0) {
            std::fprintf(stderr, "TapDevice: device disconnected. Stopping.\n");
            goto error;
        }
        
        auto read_res = ::read(m_poll.user().fd.get(), m_read_buffer.data(), m_frame_mtu);
        if (read_res <= 0) {
            bool is_error = false;
            if (read_res < 0) {
                int err = errno;
                is_error = !ErrIsEAGAINorEWOULDBLOCK(err);
            }
            if (is_error) {
                std::fprintf(stderr, "TapDevice: read failed. Stopping.\n");
                goto error;
            }
            return;
        }
        
        AIPSTACK_ASSERT(read_res <= m_frame_mtu)
        
        AIpStack::IpBufNode node{
            m_read_buffer.data(),
            (std::size_t)read_res,
            nullptr
        };
        
        frameReceived(AIpStack::IpBufRef{&node, 0, (std::size_t)read_res});
    } while (false);
    
    return;
    
error:
    uv_poll_stop(m_poll.get());
    m_active = false;
}

}
