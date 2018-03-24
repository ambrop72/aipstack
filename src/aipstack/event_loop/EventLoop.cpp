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
#include <mutex>

#include <aipstack/misc/Assert.h>
#include <aipstack/misc/OneOf.h>
#include <aipstack/misc/MinMax.h>
#include <aipstack/misc/Hints.h>
#include <aipstack/misc/Use.h>
#include <aipstack/structure/Accessor.h>
#include <aipstack/event_loop/EventLoop.h>

#if AIPSTACK_EVENT_LOOP_HAS_IOCP
#include <utility>
#include <memory>
#endif

namespace AIpStack {

struct EventLoopPriv::TimerHeapNodeAccessor :
    public MemberAccessor<EventLoopTimer, TimerHeapNode, &EventLoopTimer::m_heap_node> {};

struct EventLoopPriv::TimerCompare {
    AIPSTACK_USE_TYPES(TimerLinkModel, (State, Ref))
    AIPSTACK_USE_TYPES(EventLoop, (TimerState))

    inline static int compareEntries (State, Ref ref1, Ref ref2)
    {
        EventLoopTimer &tim1 = *ref1;
        EventLoopTimer &tim2 = *ref2;

        std::uint8_t order1 = std::uint8_t(tim1.m_state) & EventLoop::TimerStateOrderMask;
        std::uint8_t order2 = std::uint8_t(tim2.m_state) & EventLoop::TimerStateOrderMask;

        if (order1 != order2) {
            return (order1 < order2) ? -1 : 1;
        }

        if (tim1.m_time != tim2.m_time) {
            return (tim1.m_time < tim2.m_time) ? -1 : 1;
        }

        return 0;
    }

    inline static int compareKeyEntry (State, EventLoopTime time1, Ref ref2)
    {
        EventLoopTimer &tim2 = *ref2;

        AIPSTACK_ASSERT(tim2.m_state == TimerState::Pending)
        
        if (time1 != tim2.m_time) {
            return (time1 < tim2.m_time) ? -1 : 1;
        }

        return 0;
    }
};

struct EventLoopPriv::AsyncSignalNodeAccessor : public MemberAccessor<
    AsyncSignalNode, AsyncSignalListNode, &AsyncSignalNode::m_list_node> {};

EventLoopMembers::EventLoopMembers() :
    m_stop(false),
    m_event_time(EventLoop::getTime())
{
    EventLoop::AsyncSignalList::initLonely(m_pending_async_list);
    EventLoop::AsyncSignalList::initLonely(m_dispatch_async_list);    

    #if AIPSTACK_EVENT_LOOP_HAS_IOCP
    m_num_iocp_notifiers = 0;
    m_num_iocp_resources = 0;
    #endif
}

EventLoop::EventLoop () :
    EventLoopMembers(),
    EventProvider()
{}

EventLoop::~EventLoop ()
{
    AIPSTACK_ASSERT(m_timer_heap.isEmpty())
    AIPSTACK_ASSERT(AsyncSignalList::isLonely(m_pending_async_list))
    AIPSTACK_ASSERT(AsyncSignalList::isLonely(m_dispatch_async_list))
    #if AIPSTACK_EVENT_LOOP_HAS_IOCP
    AIPSTACK_ASSERT(m_num_iocp_notifiers == 0)
    #endif
    
    #if AIPSTACK_EVENT_LOOP_HAS_IOCP
    wait_for_final_iocp_results();
    #endif
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

        if (!EventProvider::dispatchEvents()) {
            return;
        }

        EventLoopTime wait_time = prepare_timers_for_wait();

        EventProvider::waitForEvents(wait_time);
    }
}

void EventLoop::prepare_timers_for_dispatch (EventLoopTime now)
{
    bool changed = false;

    m_timer_heap.findAllLesserOrEqual(now, [&](EventLoopTimer *tim) {
        AIPSTACK_ASSERT(tim->m_state == TimerState::Pending)

        tim->m_state = TimerState::Dispatch;
        changed = true;
    });

    if (changed) {
        m_timer_heap.assertValidHeap();
    }
}

bool EventLoop::dispatch_timers ()
{
    while (EventLoopTimer *tim = m_timer_heap.first()) {
        AIPSTACK_ASSERT(tim->m_state == OneOfHeapTimerStates())

        if (tim->m_state != TimerState::Dispatch) {
            break;
        }

        tim->m_state = TimerState::TempUnset;
        m_timer_heap.fixup(*tim);

        tim->handleTimerExpired();

        if (AIPSTACK_UNLIKELY(m_stop)) {
            return false;
        }
    }

    return true;
}

EventLoopTime EventLoop::prepare_timers_for_wait ()
{
    EventLoopTime first_time = EventLoopTime::max();

    while (EventLoopTimer *tim = m_timer_heap.first()) {
        AIPSTACK_ASSERT(tim->m_state == OneOf(
            TimerState::TempUnset, TimerState::TempSet, TimerState::Pending))
        
        if (tim->m_state == TimerState::TempUnset) {
            m_timer_heap.remove(*tim);
            tim->m_state = TimerState::Idle;
        }
        else if (tim->m_state == TimerState::TempSet) {
            tim->m_state = TimerState::Pending;
            m_timer_heap.fixup(*tim);
        }
        else {
            first_time = tim->m_time;
            break;
        }
    }

    return first_time;
}

bool EventLoop::dispatch_async_signals ()
{
    AIPSTACK_ASSERT(AsyncSignalList::isLonely(m_dispatch_async_list))

    std::unique_lock<std::mutex> lock(m_async_signal_mutex);

    if (AsyncSignalList::isLonely(m_pending_async_list)) {
        return true;
    }

    AsyncSignalList::initReplaceNotLonely(m_dispatch_async_list, m_pending_async_list);
    AsyncSignalList::initLonely(m_pending_async_list);

    while (true) {
        AsyncSignalNode *node = AsyncSignalList::next(m_dispatch_async_list);
        if (node == &m_dispatch_async_list) {
            break;
        }

        EventLoopAsyncSignal &asig = *static_cast<EventLoopAsyncSignal *>(node);
        AIPSTACK_ASSERT(&asig.m_loop == this)
        AIPSTACK_ASSERT(!AsyncSignalList::isRemoved(asig))

        AsyncSignalList::remove(asig);
        AsyncSignalList::markRemoved(asig);

        lock.unlock();

        asig.m_handler();

        if (AIPSTACK_UNLIKELY(m_stop)) {
            return false;
        }

        lock.lock();
    }

    return true;
}

bool EventProviderBase::getStop () const
{
    auto &event_loop = static_cast<EventLoop const &>(*this);
    return event_loop.m_stop;
}

bool EventProviderBase::dispatchAsyncSignals ()
{
    auto &event_loop = static_cast<EventLoop &>(*this);
    return event_loop.dispatch_async_signals();
}

#if AIPSTACK_EVENT_LOOP_HAS_IOCP
bool EventProviderBase::handleIocpResult (void *completion_key, OVERLAPPED *overlapped)
{
    auto &event_loop = static_cast<EventLoop &>(*this);
    return event_loop.handle_iocp_result(completion_key, overlapped);
}
#endif

EventLoopTimer::EventLoopTimer (EventLoop &loop) :
    m_loop(loop),
    m_time(EventLoopTime()),
    m_state(TimerState::Idle)
{}

EventLoopTimer::~EventLoopTimer ()
{
    if (m_state != TimerState::Idle) {
        m_loop.m_timer_heap.remove(*this);
    }
}

void EventLoopTimer::unset ()
{
    if (m_state == OneOf(TimerState::TempUnset, TimerState::TempSet)) {
        m_state = TimerState::TempUnset;
    } else {
        if (m_state != TimerState::Idle) {
            m_loop.m_timer_heap.remove(*this);
            m_state = TimerState::Idle;
        }
    }
}

void EventLoopTimer::setAt (EventLoopTime time)
{
    m_time = time;

    if (m_state == OneOf(TimerState::TempUnset, TimerState::TempSet)) {
        m_state = TimerState::TempSet;
    } else {
        TimerState old_state = m_state;
        m_state = TimerState::Pending;

        if (old_state == TimerState::Idle) {
            m_loop.m_timer_heap.insert(*this);
        } else {
            m_loop.m_timer_heap.fixup(*this);            
        }
    }
}

void EventLoopTimer::setAfter (EventLoopDuration duration)
{
    return setAt(m_loop.getEventTime() + duration);
}

#if AIPSTACK_EVENT_LOOP_HAS_FD

EventLoopFdWatcher::EventLoopFdWatcher (EventLoop &loop, FdEventHandler handler) :
    EventLoopFdWatcherMembers{
        /*m_loop=*/loop,
        /*m_handler=*/handler,
        /*m_watched_fd=*/-1,
        /*m_events=*/EventLoopFdEvents()
    },
    EventProviderFd()
{}

EventLoopFdWatcher::~EventLoopFdWatcher ()
{
    if (m_watched_fd >= 0) {
        EventProviderFd::resetImpl(m_watched_fd);
    }
}

void EventLoopFdWatcher::initFd (int fd, EventLoopFdEvents events)
{
    AIPSTACK_ASSERT(m_watched_fd == -1)
    AIPSTACK_ASSERT(fd >= 0)
    AIPSTACK_ASSERT((events & ~EventLoopFdEvents::All) == EnumZero)

    EventProviderFd::initFdImpl(fd, events);

    m_watched_fd = fd;
    m_events = events;
}

void EventLoopFdWatcher::updateEvents (EventLoopFdEvents events)
{
    AIPSTACK_ASSERT(m_watched_fd >= 0)
    AIPSTACK_ASSERT((events & ~EventLoopFdEvents::All) == EnumZero)

    if (events != m_events) {
        EventProviderFd::updateEventsImpl(m_watched_fd, events);

        m_events = events;
    }
}

void EventLoopFdWatcher::reset ()
{
    if (m_watched_fd >= 0) {
        EventProviderFd::resetImpl(m_watched_fd);

        m_watched_fd = -1;
        m_events = EventLoopFdEvents();
    }
}

EventProviderBase & EventProviderFdBase::getProvider () const
{
    auto &fd_watcher = static_cast<EventLoopFdWatcher const &>(*this);
    return fd_watcher.m_loop;
}

void EventProviderFdBase::sanityCheck () const
{
    auto &fd_watcher = static_cast<EventLoopFdWatcher const &>(*this);
    AIPSTACK_ASSERT(fd_watcher.m_watched_fd >= 0)
    AIPSTACK_ASSERT((fd_watcher.m_events & ~EventLoopFdEvents::All) == EnumZero)    
}

EventLoopFdEvents EventProviderFdBase::getFdEvents () const
{
    auto &fd_watcher = static_cast<EventLoopFdWatcher const &>(*this);
    return fd_watcher.m_events;
}

void EventProviderFdBase::callFdEventHandler (EventLoopFdEvents events)
{
    auto &fd_watcher = static_cast<EventLoopFdWatcher &>(*this);
    return fd_watcher.m_handler(events);
}

#endif

#if AIPSTACK_EVENT_LOOP_HAS_IOCP

EventLoopIocpNotifier::EventLoopIocpNotifier (EventLoop &loop, IocpEventHandler handler) :
    m_loop(loop),
    m_handler(handler),
    m_iocp_resource(nullptr),
    m_busy(false)
{
    m_loop.m_num_iocp_notifiers++;
}

EventLoopIocpNotifier::~EventLoopIocpNotifier ()
{
    reset();

    AIPSTACK_ASSERT(m_loop.m_num_iocp_notifiers > 0)
    m_loop.m_num_iocp_notifiers--;
}

void EventLoopIocpNotifier::prepare ()
{
    AIPSTACK_ASSERT(m_iocp_resource == nullptr)
    AIPSTACK_ASSERT(!m_busy)

    auto temp_iocp_resource = std::make_unique<IocpResource>();
    temp_iocp_resource->overlapped = {};
    temp_iocp_resource->loop = &m_loop;
    temp_iocp_resource->notifier = this;

    m_iocp_resource = temp_iocp_resource.release();
    m_loop.m_num_iocp_resources++;
}

void EventLoopIocpNotifier::reset ()
{
    if (m_iocp_resource != nullptr) {
        if (m_busy) {
            m_iocp_resource->notifier = nullptr;
        } else {
            AIPSTACK_ASSERT(m_loop.m_num_iocp_resources > 0)
            m_loop.m_num_iocp_resources--;
            delete m_iocp_resource;
        }

        m_iocp_resource = nullptr;
        m_busy = false;
    }
}

void EventLoopIocpNotifier::ioStarted (std::shared_ptr<void> user_resource)
{
    AIPSTACK_ASSERT(m_iocp_resource != nullptr)
    AIPSTACK_ASSERT(!m_busy)

    m_iocp_resource->user_resource = std::move(user_resource);
    m_busy = true;
}

OVERLAPPED & EventLoopIocpNotifier::getOverlapped ()
{
    AIPSTACK_ASSERT(m_iocp_resource != nullptr)

    return m_iocp_resource->overlapped;
}

bool EventLoop::addHandleToIocp (HANDLE handle, DWORD &out_error)
{
    auto iocp_res = ::CreateIoCompletionPort(
        handle, EventProvider::getIocpHandle(),
        /*CompletionKey=*/(ULONG_PTR)this, /*NumberOfConcurrentThreads=*/0);

    if (iocp_res == nullptr) {
        out_error = ::GetLastError();
        return false;
    }

    return true;
}

bool EventLoop::handle_iocp_result (void *completion_key, OVERLAPPED *overlapped)
{
    AIPSTACK_ASSERT(completion_key == this)

    IocpResource *iocp_resource = (IocpResource *)overlapped;
    AIPSTACK_ASSERT(iocp_resource->loop == this)

    iocp_resource->user_resource.reset();

    EventLoopIocpNotifier *notifier = iocp_resource->notifier;

    if (notifier == nullptr) {
        AIPSTACK_ASSERT(m_num_iocp_resources > 0)
        m_num_iocp_resources--;
        delete iocp_resource;
    } else {
        AIPSTACK_ASSERT(&notifier->m_loop == this)
        AIPSTACK_ASSERT(notifier->m_busy)
        AIPSTACK_ASSERT(notifier->m_iocp_resource == iocp_resource)

        notifier->m_busy = false;

        notifier->m_handler();
        if (AIPSTACK_UNLIKELY(m_stop)) {
            return false;
        }
    }

    return true;
}

void EventLoop::wait_for_final_iocp_results ()
{
    bool first_try = true;
    while (m_num_iocp_resources > 0) {
        if (!first_try) {
            EventProvider::waitForEvents(EventLoopTime::max());
        }
        
        first_try = false;

        EventProvider::dispatchEvents();
    }
}

#endif

EventLoopAsyncSignal::EventLoopAsyncSignal (EventLoop &loop, SignalEventHandler handler) :
    m_loop(loop),
    m_handler(handler)
{
    AsyncSignalList::markRemoved(*this);
}

EventLoopAsyncSignal::~EventLoopAsyncSignal ()
{
    reset();
}

void EventLoopAsyncSignal::signal ()
{
    bool inserted_first = false;

    {
        std::lock_guard<std::mutex> lock(m_loop.m_async_signal_mutex);

        if (AsyncSignalList::isRemoved(*this)) {
            inserted_first = AsyncSignalList::isLonely(m_loop.m_pending_async_list);
            AsyncSignalList::initBefore(*this, m_loop.m_pending_async_list);
        }
    }

    if (inserted_first) {
        m_loop.EventProvider::signalToCheckAsyncSignals();
    }
}

void EventLoopAsyncSignal::reset ()
{
    {
        std::lock_guard<std::mutex> lock(m_loop.m_async_signal_mutex);

        if (!AsyncSignalList::isRemoved(*this)) {
            AsyncSignalList::remove(*this);
            AsyncSignalList::markRemoved(*this);
        }
    }
}

}

#include AIPSTACK_EVENT_PROVIDER_IMPL_FILE
