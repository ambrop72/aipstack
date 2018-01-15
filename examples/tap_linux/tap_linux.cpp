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
#include <cstddef>
#include <stdexcept>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <linux/if_tun.h>

#include <aipstack/misc/Assert.h>
#include <aipstack/misc/Function.h>
#include <aipstack/proto/EthernetProto.h>

#include "tap_linux.h"

namespace AIpStackExamples {

TapDevice::TapDevice (AIpStack::EventLoop &loop, std::string const &device_id) :
    m_fd_watcher(loop, AIPSTACK_BIND_MEMBER(&TapDevice::handleFdEvents, this)),
    m_active(true)
{
    m_fd = AIpStack::FileDescriptorWrapper{::open("/dev/net/tun", O_RDWR)};
    if (!m_fd) {
        throw std::runtime_error("Failed to open /dev/net/tun.");
    }
    
    m_fd.setNonblocking();
    
    std::string devname_real;

    {
        struct ifreq ifr;
        std::memset(&ifr, 0, sizeof(ifr));
        ifr.ifr_flags |= IFF_NO_PI|IFF_TAP;
        std::snprintf(ifr.ifr_name, IFNAMSIZ, "%s", device_id.c_str());
        
        if (::ioctl(*m_fd, TUNSETIFF, (void *)&ifr) < 0) {
            throw std::runtime_error("ioctl(TUNSETIFF) failed.");
        }

        devname_real = ifr.ifr_name;
    }
    
    {
        AIpStack::FileDescriptorWrapper sock{::socket(AF_INET, SOCK_DGRAM, 0)};
        if (!sock) {
            throw std::runtime_error("socket(AF_INET, SOCK_DGRAM) failed.");
        }
        
        struct ifreq ifr;
        std::memset(&ifr, 0, sizeof(ifr));
        std::strcpy(ifr.ifr_name, devname_real.c_str());
        
        if (::ioctl(*sock, SIOCGIFMTU, (void *)&ifr) < 0) {
            throw std::runtime_error("ioctl(SIOCGIFMTU) failed.");
        }
        
        m_frame_mtu = ifr.ifr_mtu + AIpStack::EthHeader::Size;
    }
    
    m_read_buffer.resize(m_frame_mtu);
    m_write_buffer.resize(m_frame_mtu);
    
    m_fd_watcher.initFd(*m_fd, AIpStack::EventLoopFdEvents::Read);
}

TapDevice::~TapDevice ()
{}

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
    
    auto write_res = ::write(*m_fd, buffer, len);
    if (write_res < 0) {
        int error = errno;
        if (AIpStack::FileDescriptorWrapper::errIsEAGAINorEWOULDBLOCK(error)) {
            return AIpStack::IpErr::BUFFER_FULL;
        }
        return AIpStack::IpErr::HW_ERROR;
    }
    if ((std::size_t)write_res != len) {
        return AIpStack::IpErr::HW_ERROR;
    }
    
    return AIpStack::IpErr::SUCCESS;
}

void TapDevice::handleFdEvents (AIpStack::EventLoopFdEvents events)
{
    AIPSTACK_ASSERT(m_active)
    
    do {
        if ((events & AIpStack::EventLoopFdEvents::Error) != AIpStack::EnumZero) {
            std::fprintf(stderr, "TapDevice: Error event. Stopping.\n");
            goto error;
        }
        if ((events & AIpStack::EventLoopFdEvents::Hup) != AIpStack::EnumZero) {
            std::fprintf(stderr, "TapDevice: HUP event. Stopping.\n");
            goto error;
        }
        
        auto read_res = ::read(*m_fd, m_read_buffer.data(), m_frame_mtu);
        if (read_res <= 0) {
            bool is_error = false;
            if (read_res < 0) {
                int err = errno;
                is_error = !AIpStack::FileDescriptorWrapper::errIsEAGAINorEWOULDBLOCK(err);
            }
            if (is_error) {
                std::fprintf(stderr, "TapDevice: read failed. Stopping.\n");
                goto error;
            }
            return;
        }
        
        AIPSTACK_ASSERT(std::size_t(read_res) <= m_frame_mtu)
        
        AIpStack::IpBufNode node{
            m_read_buffer.data(),
            std::size_t(read_res),
            nullptr
        };
        
        frameReceived(AIpStack::IpBufRef{&node, 0, std::size_t(read_res)});
    } while (false);
    
    return;
    
error:
    m_fd_watcher.reset();
    m_active = false;
}

}
