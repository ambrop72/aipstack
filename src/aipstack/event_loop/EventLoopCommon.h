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

#ifndef AIPSTACK_EVENT_LOOP_COMMON_H
#define AIPSTACK_EVENT_LOOP_COMMON_H

#include <chrono>

#include <aipstack/misc/EnumBitfieldUtils.h>

/**
 * @addtogroup event-loop
 * @{
 */

#ifdef IN_DOXYGEN
#define AIPSTACK_EVENT_LOOP_HAS_FD PLATFORM_DEPENDENT
#define AIPSTACK_EVENT_LOOP_HAS_IOCP PLATFORM_DEPENDENT
#else

#if defined(__linux__)
#define AIPSTACK_EVENT_LOOP_HAS_FD 1
#else
#define AIPSTACK_EVENT_LOOP_HAS_FD 0
#endif

#if defined(_WIN32)
#define AIPSTACK_EVENT_LOOP_HAS_IOCP 1
#else
#define AIPSTACK_EVENT_LOOP_HAS_IOCP 0
#endif

#endif

/** @} */

#if AIPSTACK_EVENT_LOOP_HAS_IOCP
#include <windows.h>
#endif

namespace AIpStack {

/**
 * @addtogroup event-loop
 * @{
 */

#ifdef IN_DOXYGEN
/**
 * Type alias for the `std::chrono` clock which is used by the event loop for timers
 * (@ref EventLoopTimer).
 * 
 * Currently this is `std::chrono::steady_clock` on Linux and `std::chrono::system_clock`
 * on Windows. The rationale for such definition is:
 * - `steady_clock` is preferrable because @ref EventLoopTimer is intented for relative
 *   timing events (`steady_clock` does not jump).
 * - Windows only has high-precision timer event facilities for UTC-based clocks and not
 *   for the `QueryPerformanceCounter` clock which is what `steady_clock` is based on.
 *   Therefore `system_clock` is used on Windows, trading precision for possible issues
 *   when the clock jumps.
 */
using EventLoopClock = PLATFORM_DEPENDENT;
#else
#if defined(_WIN32)
using EventLoopClock = std::chrono::system_clock;
#else
using EventLoopClock = std::chrono::steady_clock;
#endif
#endif

/**
 * The `std::chrono::time_point` type relevant for the @ref EventLoopClock.
 */
using EventLoopTime = EventLoopClock::time_point;

/**
 * The `std::chrono::duration` type relevant for the @ref EventLoopClock.
 */
using EventLoopDuration = EventLoopClock::duration;

#ifndef IN_DOXYGEN
struct EventLoopWaitTimeoutInfo {
    EventLoopTime time;
    bool time_changed;
};
#endif

#ifndef IN_DOXYGEN
class EventProviderBase {
public:
    inline bool getStop () const;
    inline bool dispatchAsyncSignals ();
    #if AIPSTACK_EVENT_LOOP_HAS_IOCP
    inline bool handleIocpResult (void *completion_key, OVERLAPPED *overlapped);
    #endif
};
#endif

#if AIPSTACK_EVENT_LOOP_HAS_FD || defined(IN_DOXYGEN)

/**
 * Represents a set of I/O types for @ref AIpStack::EventLoopFdWatcher "EventLoopFdWatcher"
 * as a bitmask.
 * 
 * This is used in two contexts:
 * - As the requested set of events to monitor for, in @ref
 *   AIpStack::EventLoopFdWatcher::initFd "EventLoopFdWatcher::initFd" and @ref
 *   AIpStack::EventLoopFdWatcher::updateEvents "EventLoopFdWatcher::updateEvents".
 * - As the reported set of events in the @ref
 *   AIpStack::EventLoopFdWatcher::FdEventHandler
 *   "EventLoopFdWatcher::FdEventHandler" callback.
 * 
 * Note that in the current implementation, `Error` and `Hup` events are always implicitly
 * monitored and as such may be reported even if not requested (however @ref
 * AIpStack::EventLoopFdWatcher::getEvents "EventLoopFdWatcher::getEvents" would return
 * only the requested events). This is because `epoll` works that way and filtering out
 * those events could result in an infinite loop. On the other hand, the `Read` and `Write`
 * events are filtered such that they are only reported when they are requested.
 * 
 * Operators provided by @ref AIPSTACK_ENUM_BITFIELD_OPS are available.
 */
enum class EventLoopFdEvents {
    /**
     * Ready for reading.
     */
    Read  = 1 << 0,
    /**
     * Ready for writing.
     */
    Write = 1 << 1,
    /**
     * Error occurred.
     */
    Error = 1 << 2,
    /**
     * Hangup occurred.
     */
    Hup   = 1 << 3,
    /**
     * Mask of all above event types listed above.
     */
    All   = Read|Write|Error|Hup,
};
#ifndef IN_DOXYGEN
AIPSTACK_ENUM_BITFIELD_OPS(EventLoopFdEvents)
#endif

#ifndef IN_DOXYGEN
class EventProviderFdBase {
public:
    inline EventProviderBase & getProvider () const;
    inline void sanityCheck () const;
    inline EventLoopFdEvents getFdEvents () const;
    inline void callFdEventHandler (EventLoopFdEvents events);
};
#endif

#endif

/** @} */

}

#endif
