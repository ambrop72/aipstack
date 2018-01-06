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

#ifndef AIPSTACK_EVENT_LOOP_H
#define AIPSTACK_EVENT_LOOP_H

#include <cstdint>

#include <aipstack/misc/NonCopyable.h>
#include <aipstack/misc/OneOf.h>
#include <aipstack/misc/Use.h>
#include <aipstack/misc/EnumBitfieldUtils.h>
#include <aipstack/structure/TreeCompare.h>
#include <aipstack/structure/LinkModel.h>
#include <aipstack/structure/minimum/LinkedHeap.h>
#include <aipstack/structure/StructureRaiiWrapper.h>
#include <aipstack/platform_impl/EventLoopCommon.h>

#if defined(__linux__)
    #include <aipstack/platform_specific/EventProviderLinux.h>
    #define AIPSTACK_EVENT_PROVIDER EventProviderLinux
    #define AIPSTACK_EVENT_PROVIDER_SUPPORTS_FD 1
#else
    #error "Unsupported OS"
#endif

namespace AIpStack {

template <typename Callback>
using EventProvider = AIPSTACK_EVENT_PROVIDER<Callback>;

class EventLoopCallback;
class EventLoopTimer;

class EventLoop :
    private NonCopyable<EventLoop>,
    private EventProvider<EventLoopCallback>
{
    friend class EventLoopCallback;
    friend class EventLoopTimer;

    using Provider = EventProvider<EventLoopCallback>;

    struct TimerHeapNodeAccessor;
    struct TimerKeyFuncs;

    using TimerLinkModel = PointerLinkModel<EventLoopTimer>;
    using TimerCompare = TreeCompare<TimerLinkModel, TimerKeyFuncs>;
    using TimerHeap = LinkedHeap<TimerHeapNodeAccessor, TimerCompare, TimerLinkModel>;
    using TimerHeapNode = LinkedHeapNode<TimerLinkModel>;

    static int const TimerStateOrderBits = 2;
    static std::uint8_t const TimerStateOrderMask = (1 << TimerStateOrderBits) - 1;

    enum class TimerState : std::uint8_t {
        Idle       = 0,
        Dispatch   = 1,
        TempUnset  = 2,
        TempSet    = 2 | (1 << TimerStateOrderBits),
        Pending    = 3
    };

    static constexpr auto OneOfHeapTimerStates =
        OneOf(TimerState::Dispatch, TimerState::TempUnset,
              TimerState::TempSet, TimerState::Pending);

    struct TimerKey {
        EventLoopTime time;
        TimerState state;
    };
    
public:
    EventLoop ();

    ~EventLoop ();

    void stop ();

    void run ();

    inline static EventLoopTime getTime ();

    inline EventLoopTime getEventTime () const;

private:
    void prepare_timers_for_dispatch (EventLoopTime now);

    bool dispatch_timers ();

    EventLoopWaitTimeoutInfo prepare_timers_for_wait ();

private:
    StructureRaiiWrapper<TimerHeap> m_timer_heap;
    bool m_stop;
    EventLoopTime m_event_time;
    EventLoopTime m_last_wait_time;
};

class EventLoopTimer :
    private NonCopyable<EventLoopTimer>
{
    friend class EventLoop;

    AIPSTACK_USE_TYPES(EventLoop, (TimerHeapNode, TimerState, TimerKey))

public:
    EventLoopTimer (EventLoop &loop);

    ~EventLoopTimer ();

    inline bool isSet () const;

    inline EventLoopTime getSetTime () const;

    void unset ();

    void setAt (EventLoopTime time);

protected:
    virtual void handleTimerExpired () = 0;

private:
    TimerHeapNode m_heap_node;
    EventLoop *m_loop;
    TimerKey m_key;
};

EventLoopTime EventLoop::getTime ()
{
    return EventLoopClock::now();
}

EventLoopTime EventLoop::getEventTime () const
{
    return m_event_time;
}

bool EventLoopTimer::isSet () const
{
    return m_key.state != OneOf(TimerState::Idle, TimerState::TempUnset);
}

EventLoopTime EventLoopTimer::getSetTime () const
{
    return m_key.time;
}

#if AIPSTACK_EVENT_PROVIDER_SUPPORTS_FD

class EventLoopFdWatcher :
    private NonCopyable<EventLoopFdWatcher>,
    private EventProvider<EventLoopCallback>::Fd
{
    friend class EventLoopCallback;

    using ProviderFd = EventProvider<EventLoopCallback>::Fd;

public:
    EventLoopFdWatcher (EventLoop &loop);

    ~EventLoopFdWatcher ();

    inline bool hasFd () const;

    inline int getFd () const;

    inline EventLoopFdEvents getEvents () const;

    void initFd (int fd, EventLoopFdEvents events = {});

    void updateEvents (EventLoopFdEvents events);

    void reset ();

protected:
    virtual void handleFdEvents (EventLoopFdEvents events) = 0;

private:
    EventLoop *m_loop;
    int m_watched_fd;
    EventLoopFdEvents m_events;
};

bool EventLoopFdWatcher::hasFd () const
{
    return m_watched_fd >= 0;
}

int EventLoopFdWatcher::getFd () const
{
    return m_watched_fd;
}

EventLoopFdEvents EventLoopFdWatcher::getEvents () const
{
    return m_events;
}

#endif

}

#endif
