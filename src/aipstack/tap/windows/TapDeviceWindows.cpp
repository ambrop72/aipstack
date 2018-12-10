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
#include <utility>

#include <windows.h>

#include <aipstack/misc/Assert.h>
#include <aipstack/misc/Function.h>
#include <aipstack/misc/Modulo.h>
#include <aipstack/proto/EthernetProto.h>
#include <aipstack/tap/windows/TapDeviceWindows.h>
#include <aipstack/tap/windows/tapwin_funcs.h>
#include <aipstack/tap/windows/tapwin_common.h>

namespace AIpStack {

TapDeviceWindows::IoUnit::IoUnit (EventLoop &loop, TapDeviceWindows &parent) :
    m_iocp_notifier(loop, AIPSTACK_BIND_MEMBER(&IoUnit::iocpNotifierHandler, this)),
    m_parent(parent)
{}

void TapDeviceWindows::IoUnit::init (
    std::shared_ptr<WinHandleWrapper> device, std::size_t buffer_size)
{
    AIPSTACK_ASSERT(m_resource == nullptr)

    m_iocp_notifier.prepare();

    auto resource = std::make_shared<IoResource>();
    resource->device = std::move(device);
    resource->buffer.resize(buffer_size);

    m_resource = std::move(resource);    
}

void TapDeviceWindows::IoUnit::ioStarted ()
{
    m_iocp_notifier.ioStarted(std::static_pointer_cast<void>(m_resource));
}

void TapDeviceWindows::IoUnit::iocpNotifierHandler ()
{
    if (this == &m_parent.m_recv_unit) {
        return m_parent.recvCompleted(*this);
    } else {
        return m_parent.sendCompleted(*this);
    }
}

TapDeviceWindows::TapDeviceWindows (
    EventLoop &loop, std::string const &device_id, FrameReceivedHandler handler)
:
    m_handler(handler),
    m_send_first(0),
    m_send_count(0),
    m_send_units(ResourceArrayInitSame(), std::ref(loop), std::ref(*this)),
    m_recv_unit(loop, *this)
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
    
    m_device = std::make_shared<WinHandleWrapper>(::CreateFileA(
        device_path.c_str(), GENERIC_READ|GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM|FILE_FLAG_OVERLAPPED, nullptr));
    if (!*m_device) {
        throw std::runtime_error("Failed to open TAP device.");
    }
    
    DWORD len;
    
    ULONG mtu = 0;
    if (!::DeviceIoControl(**m_device, TAP_IOCTL_GET_MTU,
            &mtu, sizeof(mtu), &mtu, sizeof(mtu), &len, nullptr))
    {
        throw std::runtime_error("TAP_IOCTL_GET_MTU failed.");
    }
    
    m_frame_mtu = mtu + EthHeader::Size;
    
    ULONG status = 1;
    if (!DeviceIoControl(**m_device, TAP_IOCTL_SET_MEDIA_STATUS, &status,
            sizeof(status), &status, sizeof(status), &len, nullptr))
    {
        throw std::runtime_error("TAP_IOCTL_SET_MEDIA_STATUS failed.");
    }
    
    for (IoUnit &send_unit : m_send_units) {
        send_unit.init(m_device, m_frame_mtu);
    }

    m_recv_unit.init(m_device, m_frame_mtu);
    
    DWORD add_error;
    if (!loop.addHandleToIocp(**m_device, add_error)) {
        throw std::runtime_error("CreateIoCompletionPort failed.");
    }
    
    if (!startRecv()) {
        throw std::runtime_error("Failed to start receive operation.");
    }
}

TapDeviceWindows::~TapDeviceWindows ()
{
    if (!CancelIo(**m_device)) {
        std::fprintf(stderr, "TAP CancelIo failed (%u)!\n",
            (unsigned int)GetLastError());
    }
}

IpErr TapDeviceWindows::sendFrame (IpBufRef frame)
{
    if (frame.tot_len < EthHeader::Size) {
        return IpErr::HardwareError;
    }
    else if (frame.tot_len > m_frame_mtu) {
        return IpErr::PacketTooLarge;
    }
    
    if (m_send_count >= NumSendUnits) {
        //std::fprintf(stderr, "TAP send: out of buffers\n");
        return IpErr::OutputBufferFull;
    }
    
    std::size_t unit_index = Modulo(NumSendUnits).add(m_send_first, m_send_count);
    
    IoUnit &send_unit = m_send_units[unit_index];
    AIPSTACK_ASSERT(!send_unit.m_iocp_notifier.isBusy())
    
    char *buffer = send_unit.m_resource->buffer.data();
    
    std::size_t len = frame.tot_len;
    frame.takeBytes(len, buffer);
    
    OVERLAPPED &olap = send_unit.m_iocp_notifier.getOverlapped();
    std::memset(&olap, 0, sizeof(olap));
    
    bool res = ::WriteFile(**m_device, buffer, len, nullptr, &olap);
    DWORD error;
    if (!res && (error = ::GetLastError()) != ERROR_IO_PENDING) {
        std::fprintf(stderr, "TAP WriteFile failed (err=%u)!\n",
            (unsigned int)error);
        return IpErr::HardwareError;
    }
    
    send_unit.ioStarted();
    m_send_count++;
    
    return IpErr::Success;
}

bool TapDeviceWindows::startRecv ()
{
    IoUnit &recv_unit = m_recv_unit;
    AIPSTACK_ASSERT(!recv_unit.m_iocp_notifier.isBusy())
    
    char *buffer = recv_unit.m_resource->buffer.data();
    
    OVERLAPPED &olap = recv_unit.m_iocp_notifier.getOverlapped();
    std::memset(&olap, 0, sizeof(olap));
    
    bool res = ::ReadFile(**m_device, buffer, m_frame_mtu, nullptr, &olap);
    DWORD error;
    if (!res && (error = ::GetLastError()) != ERROR_IO_PENDING) {
        std::fprintf(stderr, "TAP ReadFile failed (err=%u)!\n",
            (unsigned int)error);
        return false;
    }
    
    recv_unit.ioStarted();
    
    return true;
}

void TapDeviceWindows::sendCompleted (IoUnit &send_unit)
{
    std::size_t index = &send_unit - &m_send_units[0];
    (void)index;
    AIPSTACK_ASSERT(index >= 0 && index < NumSendUnits)
    AIPSTACK_ASSERT(Modulo(NumSendUnits).sub(index, m_send_first) < m_send_count)
    
    OVERLAPPED &olap = send_unit.m_iocp_notifier.getOverlapped();
    
    DWORD bytes;
    if (!::GetOverlappedResult(**m_device, &olap, &bytes, false)) {
        std::fprintf(stderr, "TAP WriteFile async failed (err=%u)!\n",
            (unsigned int)::GetLastError());
    } else {
        AIPSTACK_ASSERT(bytes >= EthHeader::Size)
        AIPSTACK_ASSERT(bytes <= m_frame_mtu)
    }
    
    while (m_send_count > 0 && !m_send_units[m_send_first].m_iocp_notifier.isBusy()) {
        m_send_first = Modulo(NumSendUnits).inc(m_send_first);
        m_send_count--;
    }
}

void TapDeviceWindows::recvCompleted (IoUnit &recv_unit)
{
    AIPSTACK_ASSERT(&recv_unit == &m_recv_unit)

    OVERLAPPED &olap = recv_unit.m_iocp_notifier.getOverlapped();
    
    DWORD bytes;
    if (!::GetOverlappedResult(**m_device, &olap, &bytes, false)) {
        std::fprintf(stderr, "TAP ReadFile async failed (err=%u)!\n",
            (unsigned int)::GetLastError());
        return;
    }
    
    AIPSTACK_ASSERT(bytes <= m_frame_mtu)

    char *buffer = recv_unit.m_resource->buffer.data();
    
    IpBufNode node{buffer, (std::size_t)bytes, nullptr};
    
    m_handler(IpBufRef{&node, 0, (std::size_t)bytes});
    
    startRecv();
}

}
