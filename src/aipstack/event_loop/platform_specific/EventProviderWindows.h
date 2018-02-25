/*
 * Copyright (c) 2018 Ambroz Bizjak
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

#ifndef AIPSTACK_EVENT_PROVIDER_WINDOWS_H
#define AIPSTACK_EVENT_PROVIDER_WINDOWS_H

#include <cstdint>

#include <windows.h>

#include <aipstack/misc/NonCopyable.h>
#include <aipstack/platform/WinHandleWrapper.h>
#include <aipstack/event_loop/EventLoopCommon.h>

namespace AIpStack {

class EventProviderWindows :
    public EventProviderBase,
    private NonCopyable<EventProviderWindows>
{
    static int const MaxIocpEvents = 64;

public:
    EventProviderWindows ();

    ~EventProviderWindows ();

    void waitForEvents (EventLoopWaitTimeoutInfo timeout_info);

    bool dispatchEvents ();

    void signalToCheckAsyncSignals ();

    inline HANDLE getIocpHandle () const { return *m_iocp_handle; }

private:
    static void CALLBACK timerApcCallbackTrampoline(void *arg, DWORD lowVal, DWORD highVal);
    void timerApcCallback(DWORD lowVal, DWORD highVal);

private:
    WinHandleWrapper m_iocp_handle;
    WinHandleWrapper m_timer_handle;
    int m_cur_iocp_event;
    int m_num_iocp_events;
    bool m_force_timer_update;
    OVERLAPPED m_async_signal_overlapped;
    OVERLAPPED_ENTRY m_iocp_events[MaxIocpEvents];
};

using EventProvider = EventProviderWindows;

#define AIPSTACK_EVENT_PROVIDER_IMPL_FILE \
    <aipstack/event_loop/platform_specific/EventProviderWindows_impl.h>

}

#endif
