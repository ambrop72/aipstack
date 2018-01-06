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

#include <aipstack/misc/Assert.h>
#include <aipstack/misc/OneOf.h>
#include <aipstack/misc/MinMax.h>
#include <aipstack/misc/Hints.h>
#include <aipstack/misc/Use.h>
#include <aipstack/structure/Accessor.h>
#include <aipstack/platform_impl/EventLoop.h>

namespace AIpStack {

struct EventLoop::TimerHeapNodeAccessor :
    public MemberAccessor<EventLoopTimer, TimerHeapNode, &EventLoopTimer::m_heap_node> {};

struct EventLoop::TimerKeyFuncs {
    inline static TimerKey GetKeyOfEntry (EventLoopTimer const &tim)
    {
        return tim.m_key;
    }

    static int CompareKeys (TimerKey key1, TimerKey key2)
    {
        std::uint8_t order1 = std::uint8_t(key1.state) & TimerStateOrderMask;
        std::uint8_t order2 = std::uint8_t(key2.state) & TimerStateOrderMask;

        if (order1 != order2) {
            return (order1 < order2) ? -1 : 1;
        }

        if (key1.time != key2.time) {
            return (key1.time < key2.time) ? -1 : 1;
        }

        return 0;
    }

    static int CompareKeys (EventLoopTime time1, TimerKey key2)
    {
        AIPSTACK_ASSERT(key2.state == TimerState::Pending)
        
        if (time1 != key2.time) {
            return (time1 < key2.time) ? -1 : 1;
        }

        return 0;
    }
};

EventLoop::EventLoop () :
    m_stop(false),
    m_event_time(getTime()),
    m_last_wait_time(EventLoopTime::max())
{}

EventLoop::~EventLoop ()
{
    AIPSTACK_ASSERT(m_timer_heap.isEmpty())
}

void EventLoop::stop ()
{
    m_stop = true;
}

void EventLoop::run ()
{
    if (m_stop) {
        return;
    }

    while (true) {
        m_event_time = getTime();

        prepare_timers_for_dispatch(m_event_time);

        if (!dispatch_timers()) {
            return;
        }

        if (!Provider::dispatchSystemEvents()) {
            return;
        }

        EventLoopWaitTimeoutInfo timeout_info = prepare_timers_for_wait();

        Provider::waitForEvents(timeout_info);
    }
}

void EventLoop::prepare_timers_for_dispatch (EventLoopTime now)
{
    m_timer_heap.findAllLesserOrEqual(now, [&](EventLoopTimer *tim) {
        AIPSTACK_ASSERT(tim->m_key.state == TimerState::Pending)
        tim->m_key.state = TimerState::Dispatch;
    });

    m_timer_heap.assertValidHeap();
}

bool EventLoop::dispatch_timers ()
{
    while (EventLoopTimer *tim = m_timer_heap.first()) {
        AIPSTACK_ASSERT(tim->m_key.state == OneOfHeapTimerStates)

        if (tim->m_key.state != TimerState::Dispatch) {
            break;
        }

        tim->m_key.state = TimerState::TempUnset;
        m_timer_heap.fixup(*tim);

        tim->handleTimerExpired();

        if (AIPSTACK_UNLIKELY(m_stop)) {
            return false;
        }
    }

    return true;
}

EventLoopWaitTimeoutInfo EventLoop::prepare_timers_for_wait ()
{
    EventLoopTime first_time = EventLoopTime::max();

    while (EventLoopTimer *tim = m_timer_heap.first()) {
        AIPSTACK_ASSERT(tim->m_key.state == OneOf(
            TimerState::TempUnset, TimerState::TempSet, TimerState::Pending))
        
        if (tim->m_key.state == TimerState::TempUnset) {
            m_timer_heap.remove(*tim);
            tim->m_key.state = TimerState::Idle;
        }
        else if (tim->m_key.state == TimerState::TempSet) {
            tim->m_key.state = TimerState::Pending;
            m_timer_heap.fixup(*tim);
        }
        else {
            first_time = tim->m_key.time;
            break;
        }
    }

    bool time_changed = (first_time != m_last_wait_time);
    m_last_wait_time = first_time;

    return {first_time, time_changed};
}

EventLoopTimer::EventLoopTimer (EventLoop &loop) :
    m_loop(&loop),
    m_key{EventLoopTime(), TimerState::Idle}
{}

EventLoopTimer::~EventLoopTimer ()
{
    if (m_key.state != TimerState::Idle) {
        m_loop->m_timer_heap.remove(*this);
    }
}

void EventLoopTimer::unset ()
{
    if (m_key.state == OneOf(TimerState::TempUnset, TimerState::TempSet)) {
        m_key.state = TimerState::TempUnset;
    } else {
        if (m_key.state != TimerState::Idle) {
            m_loop->m_timer_heap.remove(*this);
            m_key.state = TimerState::Idle;
        }
    }
}

void EventLoopTimer::setAt (EventLoopTime time)
{
    m_key.time = time;

    if (m_key.state == OneOf(TimerState::TempUnset, TimerState::TempSet)) {
        m_key.state = TimerState::TempSet;
    } else {
        TimerState old_state = m_key.state;
        m_key.state = TimerState::Pending;

        if (old_state == TimerState::Idle) {
            m_loop->m_timer_heap.insert(*this);
        } else {
            m_loop->m_timer_heap.fixup(*this);            
        }
    }
}

class EventLoopCallback {
    AIPSTACK_USE_TYPES(EventLoop, (Provider))

public:
    static bool getStop (Provider const &prov)
    {
        return static_cast<EventLoop const &>(prov).m_stop;
    }

    #if AIPSTACK_EVENT_PROVIDER_SUPPORTS_FD

    static void fdSanityCheck (Provider::Fd const &prov_fd)
    {
        auto &fdw = static_cast<EventLoopFdWatcher const &>(prov_fd);
        AIPSTACK_ASSERT(fdw.m_watched_fd >= 0)
        AIPSTACK_ASSERT((fdw.m_events & ~EventLoopFdEvents::All) == EnumZero)
    }

    static EventLoopFdEvents fdGetEvents (Provider::Fd const &prov_fd)
    {
        auto &fdw = static_cast<EventLoopFdWatcher const &>(prov_fd);
        return fdw.m_events;
    }

    static void fdCallHandler (Provider::Fd &prov_fd, EventLoopFdEvents events)
    {
        return static_cast<EventLoopFdWatcher &>(prov_fd).handleFdEvents(events);
    }

    static Provider & fdGetProvider (Provider::Fd const &prov_fd)
    {
        auto &fdw = static_cast<EventLoopFdWatcher const &>(prov_fd);
        return *fdw.m_loop;
    }

    #endif
};

#if AIPSTACK_EVENT_PROVIDER_SUPPORTS_FD

EventLoopFdWatcher::EventLoopFdWatcher (EventLoop &loop) :
    m_loop(&loop),
    m_watched_fd(-1),
    m_events(EventLoopFdEvents())
{}

EventLoopFdWatcher::~EventLoopFdWatcher ()
{
    if (m_watched_fd >= 0) {
        ProviderFd::resetImpl(m_watched_fd);
    }
}

void EventLoopFdWatcher::initFd (int fd, EventLoopFdEvents events)
{
    AIPSTACK_ASSERT(m_watched_fd == -1)
    AIPSTACK_ASSERT(fd >= 0)
    AIPSTACK_ASSERT((events & ~EventLoopFdEvents::All) == EnumZero)

    ProviderFd::initFdImpl(fd, events);

    m_watched_fd = fd;
    m_events = events;
}

void EventLoopFdWatcher::updateEvents (EventLoopFdEvents events)
{
    AIPSTACK_ASSERT(m_watched_fd >= 0)
    AIPSTACK_ASSERT((events & ~EventLoopFdEvents::All) == EnumZero)

    if (events != m_events) {
        ProviderFd::updateEventsImpl(m_watched_fd, events);

        m_events = events;
    }
}

void EventLoopFdWatcher::reset ()
{
    if (m_watched_fd >= 0) {
        ProviderFd::resetImpl(m_watched_fd);

        m_watched_fd = -1;
        m_events = EventLoopFdEvents();
    }
}

#endif

}

#if defined(__linux__)
#include <aipstack/platform_specific/EventProviderLinux_impl.h>
#endif
