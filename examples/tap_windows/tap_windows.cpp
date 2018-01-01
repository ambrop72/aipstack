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
#include <stdexcept>

#include <uv.h>

#include <windows.h>

#include <aipstack/misc/Assert.h>
#include <aipstack/proto/EthernetProto.h>

#include "tapwin_funcs.h"
#include "tapwin_common.h"
#include "tap_windows.h"

namespace AIpStackExamples {

TapDevice::TapDevice (uv_loop_t *loop, std::string const &device_id) :
    m_loop(loop),
    m_send_first(0),
    m_send_count(0)
{
    std::string component_id;
    std::string device_name;
    if (!tapwin_parse_tap_spec(device_id, component_id, device_name)) {
        throw std::runtime_error("Failed to parse TAP specification.");
    }
    
    std::string device_path;
    if (!tapwin_find_device(component_id, device_name, device_path)) {
        throw std::runtime_error("Failed to find TAP device.");
    }
    
    m_device = std::make_shared<AIpStack::WinHandleWrapper>(CreateFileA(
        device_path.c_str(), GENERIC_READ|GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM|FILE_FLAG_OVERLAPPED, nullptr));
    if (m_device->get() == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Failed to open TAP device.");
    }
    
    DWORD len;
    
    ULONG mtu = 0;
    if (!DeviceIoControl(m_device->get(), TAP_IOCTL_GET_MTU, &mtu, sizeof(mtu),
                         &mtu, sizeof(mtu), &len, nullptr))
    {
        throw std::runtime_error("TAP_IOCTL_GET_MTU failed.");
    }
    
    m_frame_mtu = mtu + AIpStack::EthHeader::Size;
    
    ULONG status = 1;
    if (!DeviceIoControl(m_device->get(), TAP_IOCTL_SET_MEDIA_STATUS, &status,
                         sizeof(status), &status, sizeof(status), &len, nullptr))
    {
        throw std::runtime_error("TAP_IOCTL_SET_MEDIA_STATUS failed.");
    }
    
    for (OverlappedWrapper &wrapper : m_uv_olap_send) {
        initOverlapped(wrapper);
    }
    
    initOverlapped(m_uv_olap_recv);
    
    HANDLE loop_iocp = uv_overlapped_get_iocp(m_uv_olap_recv.get());
    
    if (!CreateIoCompletionPort(m_device->get(), loop_iocp, 0, 0)) {
        throw std::runtime_error("CreateIoCompletionPort failed.");
    }
    
    if (!startRecv()) {
        throw std::runtime_error("Failed to start receive operation.");
    }
}

TapDevice::~TapDevice ()
{
    if (!CancelIo(m_device->get())) {
        std::fprintf(stderr, "TAP CancelIo failed (%u)!\n",
                     (unsigned int)GetLastError());
    }
    
    // Buffers associated with ongoing I/O operations are part of OverlappedUserData
    // within OverlappedWrapper so they will only be freed by UvHandleWrapper after
    // those I/O operations complete. Our callbacks (olapSendCb, olapRecvCb) will
    // not be called any more because the OverlappedWrapper destructor will call
    // uv_close on uv_overlapped_t and uv_overlapped_t ensures the callbacks are
    // not called after that.
}

std::size_t TapDevice::getMtu () const
{
    return m_frame_mtu;
}

AIpStack::IpErr TapDevice::sendFrame (AIpStack::IpBufRef frame)
{
    if (frame.tot_len < AIpStack::EthHeader::Size) {
        return AIpStack::IpErr::HW_ERROR;
    }
    else if (frame.tot_len > m_frame_mtu) {
        return AIpStack::IpErr::PKT_TOO_LARGE;
    }
    
    if (m_send_count >= NumSendBuffers) {
        //std::fprintf(stderr, "TAP send: out of buffers\n");
        return AIpStack::IpErr::BUFFER_FULL;
    }
    
    std::size_t buf_index = send_ring_add(m_send_first, m_send_count);
    
    OverlappedWrapper &wrapper = m_uv_olap_send[buf_index];
    AIPSTACK_ASSERT(!wrapper.user().active)
    
    char *buffer = wrapper.user().buffer.data();
    
    std::size_t len = frame.tot_len;
    frame.takeBytes(len, buffer);
    
    OVERLAPPED *olap = uv_overlapped_get_overlapped(wrapper.get());
    std::memset(olap, 0, sizeof(*olap));
    
    bool res = WriteFile(m_device->get(), buffer, len, nullptr, olap);
    DWORD error;
    if (!res && (error = GetLastError()) != ERROR_IO_PENDING) {
        std::fprintf(stderr, "TAP WriteFile failed (err=%u)!\n",
                     (unsigned int)error);
        return AIpStack::IpErr::HW_ERROR;
    }
    
    uv_overlapped_start(wrapper.get(),
                        &TapDevice::olapCbTrampoline<&TapDevice::olapSendCb>);
    
    wrapper.user().active = true;
    m_send_count++;
    
    return AIpStack::IpErr::SUCCESS;
}

std::size_t TapDevice::send_ring_add (std::size_t a, std::size_t b)
{
    return (a + b) % NumSendBuffers;
}

std::size_t TapDevice::send_ring_sub (std::size_t a, std::size_t b)
{
    return (NumSendBuffers + a - b) % NumSendBuffers;
}

void TapDevice::initOverlapped (OverlappedWrapper &wrapper)
{
    if (wrapper.initialize([&](uv_overlapped_t *dst) {
        return uv_overlapped_init(m_loop, dst);
    }) != 0) {
        throw std::runtime_error("uv_overlapped_init failed.");
    }
    
    wrapper.get()->data = &wrapper;
    
    wrapper.user().parent = this;
    wrapper.user().device = m_device;
    wrapper.user().buffer.resize(m_frame_mtu);
    wrapper.user().active = false;
}

bool TapDevice::startRecv ()
{
    OverlappedWrapper &wrapper = m_uv_olap_recv;
    AIPSTACK_ASSERT(!wrapper.user().active)
    
    char *buffer = wrapper.user().buffer.data();
    
    OVERLAPPED *olap = uv_overlapped_get_overlapped(wrapper.get());
    std::memset(olap, 0, sizeof(*olap));
    
    bool res = ReadFile(m_device->get(), buffer, m_frame_mtu, nullptr, olap);
    DWORD error;
    if (!res && (error = GetLastError()) != ERROR_IO_PENDING) {
        std::fprintf(stderr, "TAP ReadFile failed (err=%u)!\n",
                     (unsigned int)error);
        return false;
    }
    
    uv_overlapped_start(wrapper.get(),
                        &TapDevice::olapCbTrampoline<&TapDevice::olapRecvCb>);
    
    wrapper.user().active = true;
    
    return true;
}

template <void (TapDevice::*Cb) (TapDevice::OverlappedWrapper &)>
void TapDevice::olapCbTrampoline (uv_overlapped_t *handle)
{
    OverlappedWrapper *wrapper = reinterpret_cast<OverlappedWrapper *>(handle->data);
    AIPSTACK_ASSERT(handle == wrapper->get())
    TapDevice &parent = *wrapper->user().parent;
    
    (parent.*Cb)(*wrapper);
}

void TapDevice::olapSendCb (OverlappedWrapper &wrapper)
{
    std::size_t index = &wrapper - m_uv_olap_send;
    (void)index;
    AIPSTACK_ASSERT(index >= 0 && index < NumSendBuffers)
    AIPSTACK_ASSERT(send_ring_sub(index, m_send_first) < m_send_count)
    AIPSTACK_ASSERT(wrapper.user().active)
    
    wrapper.user().active = false;
    
    OVERLAPPED *olap = uv_overlapped_get_overlapped(wrapper.get());
    
    DWORD bytes;
    if (!GetOverlappedResult(m_device->get(), olap, &bytes, false)) {
        std::fprintf(stderr, "TAP WriteFile async failed (err=%u)!\n",
                     (unsigned int)GetLastError());
    } else {
        AIPSTACK_ASSERT(bytes >= AIpStack::EthHeader::Size)
        AIPSTACK_ASSERT(bytes <= m_frame_mtu)
    }
    
    while (m_send_count > 0 && !m_uv_olap_send[m_send_first].user().active) {
        m_send_first = send_ring_add(m_send_first, 1);
        m_send_count--;
    }
}

void TapDevice::olapRecvCb (OverlappedWrapper &wrapper)
{
    AIPSTACK_ASSERT(&wrapper == &m_uv_olap_recv)
    AIPSTACK_ASSERT(wrapper.user().active)
    
    wrapper.user().active = false;
    
    OVERLAPPED *olap = uv_overlapped_get_overlapped(wrapper.get());
    
    DWORD bytes;
    if (!GetOverlappedResult(m_device->get(), olap, &bytes, false)) {
        std::fprintf(stderr, "TAP ReadFile async failed (err=%u)!\n",
                     (unsigned int)GetLastError());
        return;
    }
    
    AIPSTACK_ASSERT(bytes <= m_frame_mtu)
    
    AIpStack::IpBufNode node{
        wrapper.user().buffer.data(),
        (std::size_t)bytes,
        nullptr
    };
    
    frameReceived(AIpStack::IpBufRef{&node, 0, (std::size_t)bytes});
    
    startRecv();
}

}
