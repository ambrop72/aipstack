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

#include <cstdint>
#include <cstdio>
#include <stdexcept>

#include <windows.h>

#include <aipstack/misc/MinMax.h>
#include <aipstack/event_loop/platform_specific/EventProviderWindows.h>

namespace AIpStack {

EventProviderWindows::EventProviderWindows () :
    m_cur_iocp_event(0),
    m_num_iocp_events(0),
    m_force_timer_update(true),
    m_async_signal_overlapped{}
{
    m_iocp_handle = WinHandleWrapper(
        ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, (ULONG_PTR)nullptr, 1));
    if (!m_iocp_handle) {
        throw std::runtime_error("CreateIoCompletionPort failed");
    }

    // Note: manual reset choice should not matter for out use.
    m_timer_handle = WinHandleWrapper(
        ::CreateWaitableTimer(nullptr, /*bManualReset=*/true, nullptr));
    if (!m_timer_handle) {
        throw std::runtime_error("CreateWaitableTimer failed");
    }
}

EventProviderWindows::~EventProviderWindows ()
{
    // Use CancelWaitableTimer to make sure that timerApcCallbackTrampoline will not
    // run after we close the timer. This function is documented to not only stop the
    // timer but also cancel outstanding APCs.
    if (!::CancelWaitableTimer(*m_timer_handle)) {
        std::fprintf(stderr, "EventProviderWindows::~EventProviderWindows: "
            "CancelWaitableTimer failed!");
    }
}

void EventProviderWindows::waitForEvents (EventLoopWaitTimeoutInfo timeout_info)
{
    AIPSTACK_ASSERT(m_cur_iocp_event == m_num_iocp_events)

    if (timeout_info.time_changed || m_force_timer_update) {
        m_force_timer_update = true;

        LARGE_INTEGER due_time;
        due_time.QuadPart = MaxValue(
            timeout_info.time.time_since_epoch().count(), EventLoopTime::rep(1));
        
        bool timer_res = ::SetWaitableTimer(
            *m_timer_handle, &due_time, /*lPeriod=*/0,
            &EventProviderWindows::timerApcCallbackTrampoline,
            /*lpArgToCompletionRoutine=*/this, /*fResume=*/false);
        if (!timer_res) {
            throw std::runtime_error("SetWaitableTimer failed");
        }

        m_force_timer_update = false;
    }

    unsigned long num_events = 0;
    bool wait_result = ::GetQueuedCompletionStatusEx(
        *m_iocp_handle, m_iocp_events, MaxIocpEvents, &num_events,
        /*dwMilliseconds=*/0xFFFFFFFF, /*fAlertable=*/true);
    
    if (!wait_result) {
        throw std::runtime_error("GetQueuedCompletionStatusEx failed");
    }

    AIPSTACK_ASSERT(num_events <= MaxIocpEvents)

    m_cur_iocp_event = 0;
    m_num_iocp_events = num_events;
}

bool EventProviderWindows::dispatchEvents ()
{
    while (m_cur_iocp_event < m_num_iocp_events) {
        OVERLAPPED_ENTRY &event = m_iocp_events[m_cur_iocp_event++];

        if (event.lpCompletionKey == (ULONG_PTR)&m_async_signal_overlapped) {
            AIPSTACK_ASSERT(event.lpOverlapped == &m_async_signal_overlapped)

            if (!EventProviderBase::dispatchAsyncSignals()) {
                return false;
            }

            continue;
        }

        if (!EventProviderBase::handleIocpResult(
            (void *)event.lpCompletionKey, event.lpOverlapped))
        {
            return false;
        }
    }

    return true;
}

void EventProviderWindows::signalToCheckAsyncSignals ()
{
    if (!::PostQueuedCompletionStatus(
        *m_iocp_handle, /*dwNumberOfBytesTransferred=*/0, 
        /*dwCompletionKey=*/(ULONG_PTR)&m_async_signal_overlapped,
        /*lpOverlapped=*/&m_async_signal_overlapped))
    {
        std::fprintf(stderr, "EventProviderWindows::signalToCheckAsyncSignals: "
            "PostQueuedCompletionStatus failed!");
    }
}

void EventProviderWindows::timerApcCallbackTrampoline(
    void *arg, DWORD lowVal, DWORD highVal)
{
    return static_cast<EventProviderWindows *>(arg)->timerApcCallback(lowVal, highVal);
}

void EventProviderWindows::timerApcCallback(DWORD lowVal, DWORD highVal)
{
    (void)lowVal;
    (void)highVal;

    m_force_timer_update = true;
}

}
