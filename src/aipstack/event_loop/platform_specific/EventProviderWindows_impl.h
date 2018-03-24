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
#include <type_traits>

#include <windows.h>

#include <aipstack/misc/MinMax.h>
#include <aipstack/event_loop/FormatString.h>
#include <aipstack/event_loop/platform_specific/EventProviderWindows.h>

namespace AIpStack {

EventProviderWindows::EventProviderWindows () :
    m_cur_iocp_event(0),
    m_num_iocp_events(0),
    m_timer_time(EventLoopTime::max()),
    m_force_timer_update(true),
    m_async_signal_overlapped{}
{
    m_iocp_handle = WinHandleWrapper(
        ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, (ULONG_PTR)nullptr, 1));
    if (!m_iocp_handle) {
        throw std::runtime_error(formatString(
            "EventProviderWindows: CreateIoCompletionPort failed, err=%u",
            (unsigned int)::GetLastError()));
    }

    // Note: manual reset choice should not matter for out use.
    m_timer_handle = WinHandleWrapper(
        ::CreateWaitableTimer(nullptr, /*bManualReset=*/true, nullptr));
    if (!m_timer_handle) {
        throw std::runtime_error(formatString(
            "EventProviderWindows: CreateWaitableTimer failed, err=%u",
            (unsigned int)::GetLastError()));
    }
}

EventProviderWindows::~EventProviderWindows ()
{
    // Use CancelWaitableTimer to make sure that timerApcCallbackTrampoline will not
    // run after we close the timer. This function is documented to not only stop the
    // timer but also cancel outstanding APCs.
    if (!::CancelWaitableTimer(*m_timer_handle)) {
        std::fprintf(stderr, "EventProviderWindows: CancelWaitableTimer failed, err=%u\n",
            (unsigned int)::GetLastError());
    }
}

void EventProviderWindows::waitForEvents (EventLoopTime wait_time)
{
    AIPSTACK_ASSERT(m_cur_iocp_event == m_num_iocp_events)

    // We assume that std::system_clock which EventLoop timestamps are based on has
    // 100us period. This means that conversion to time needed by SetWaitableTimer
    // only needs an offset (see below).
    static_assert(EventLoopDuration::period::num == 1, "");
    static_assert(EventLoopDuration::period::den == 10000000, "");

    // We assume that the system_clock uses a signed time representation. This is
    // needed to be able to do the clamping without complications (see below).
    static_assert(std::is_signed<EventLoopDuration::rep>::value, "");

    // Offset which needs to be added to std::system_clock time (Unix epoch) to
    // obtain time for use with SetWaitableTimer (1601 epoch). The unit is 100us.
    LONGLONG const UnixToFileTimeOffset = 116444736000000000;

    if (wait_time != m_timer_time || m_force_timer_update) {
        m_force_timer_update = true;

        // Get the tick count from the system_clock-based timeout.
        auto unix_time = wait_time.time_since_epoch().count();

        // Convert time. The general case is the last one but the first two checks
        // effectively clamp the result to [1, MAX_LONGLONG].
        LARGE_INTEGER due_file_time;
        if (unix_time <= -UnixToFileTimeOffset) {
            // Non-positive timeout would have been be interpreted as relative,
            // clamp to 1.
            due_file_time.QuadPart = 1;
        }
        else if (unix_time > TypeMax<LONGLONG>() - UnixToFileTimeOffset) {
            // There would have been an integer overflow, clamp to MAX_LONGLONG.
            due_file_time.QuadPart = TypeMax<LONGLONG>();
        }
        else {
            // Normal case, add offset.
            due_file_time.QuadPart = UnixToFileTimeOffset + unix_time;
        }

        bool timer_res = ::SetWaitableTimer(
            *m_timer_handle, &due_file_time, /*lPeriod=*/0,
            &EventProviderWindows::timerApcCallbackTrampoline,
            /*lpArgToCompletionRoutine=*/this, /*fResume=*/false);
        if (!timer_res) {
            throw std::runtime_error(formatString(
                "EventProviderWindows: SetWaitableTimer failed, err=%u",
                (unsigned int)::GetLastError()));
        }

        m_timer_time = wait_time;
        m_force_timer_update = false;
    }

    unsigned long num_events = 0;
    bool wait_result = ::GetQueuedCompletionStatusEx(
        *m_iocp_handle, m_iocp_events, MaxIocpEvents, &num_events,
        /*dwMilliseconds=*/0xFFFFFFFF, /*fAlertable=*/true);
    
    if (!wait_result) {
        auto err = ::GetLastError();
        if (err == WAIT_IO_COMPLETION) {
            // This return code is undocumented but actually occurs when APC was run
            // and must be checked. In this case num_events would be set to 1 but this
            // is spurious and we need to force it back to 0.
            num_events = 0;
        } else {
            throw std::runtime_error(formatString(
                "EventProviderWindows: GetQueuedCompletionStatusEx failed, err=%u",
                (unsigned int)err));
        }
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
        std::fprintf(stderr,
            "EventProviderWindows: PostQueuedCompletionStatus failed, err=%u\n",
            (unsigned int)::GetLastError());
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
