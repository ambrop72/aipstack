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

#ifndef AIPSTACK_TAP_DEVICE_WINDOWS_H
#define AIPSTACK_TAP_DEVICE_WINDOWS_H

#include <cstddef>
#include <string>
#include <memory>
#include <vector>
#include <functional>

#include <aipstack/misc/NonCopyable.h>
#include <aipstack/misc/Function.h>
#include <aipstack/misc/platform_specific/WinHandleWrapper.h>
#include <aipstack/misc/ResourceArray.h>
#include <aipstack/infra/Err.h>
#include <aipstack/infra/Buf.h>
#include <aipstack/event_loop/EventLoop.h>

namespace AIpStack {

class TapDeviceWindows :
    private NonCopyable<TapDeviceWindows>
{
    inline static constexpr std::size_t NumSendUnits = 16;
    
    struct IoResource {
        std::shared_ptr<WinHandleWrapper> device;
        std::vector<char> buffer;
    };

    struct IoUnit {
        IoUnit (EventLoop &loop, TapDeviceWindows &parent);

        void init (std::shared_ptr<WinHandleWrapper> device, std::size_t buffer_size);

        void ioStarted ();
        
        void iocpNotifierHandler ();

        EventLoopIocpNotifier m_iocp_notifier;
        TapDeviceWindows &m_parent;
        std::shared_ptr<IoResource> m_resource;
    };
    
public:
    using FrameReceivedHandler = Function<void(AIpStack::IpBufRef frame)>;
    
    TapDeviceWindows (EventLoop &loop, std::string const &device_id,
                      FrameReceivedHandler handler);

    ~TapDeviceWindows ();
    
    std::size_t getMtu () const {
        return m_frame_mtu;
    }

    IpErr sendFrame (IpBufRef frame);
    
private:
    bool startRecv ();
    void sendCompleted (IoUnit &send_unit);
    void recvCompleted (IoUnit &recv_unit);
    
private:
    FrameReceivedHandler m_handler;
    std::shared_ptr<WinHandleWrapper> m_device;
    std::size_t m_frame_mtu;
    std::size_t m_send_first;
    std::size_t m_send_count;
    ResourceArray<IoUnit, NumSendUnits> m_send_units;
    IoUnit m_recv_unit;    
};

}

#endif
