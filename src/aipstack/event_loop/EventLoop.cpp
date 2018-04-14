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
#include <cstdio>
#include <utility>
#include <memory>
#include <stdexcept>
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

        if (tim1.m_state != tim2.m_state) {
            return (tim1.m_state < tim2.m_state) ? -1 : 1;
        }

        if (tim1.m_time != tim2.m_time) {
            return (tim1.m_time < tim2.m_time) ? -1 : 1;
        }

        return 0;
    }

    inline static int compareKeyEntry (State, EventLoopTime time1, Ref ref2)
    {
        TimerState state1 = TimerState::Pending;
        EventLoopTimer &tim2 = *ref2;

        if (state1 != tim2.m_state) {
            return (state1 < tim2.m_state) ? -1 : 1;
        }
        
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
    m_recheck_async_signals(false),
    m_event_time(EventLoop::getTime()),
    m_num_timers(0),
    m_num_async_signals(0)
    #if AIPSTACK_EVENT_LOOP_HAS_FD
    ,m_num_fd_notifiers(0)
    #endif
    #if AIPSTACK_EVENT_LOOP_HAS_IOCP
    ,m_num_iocp_notifiers(0)
    ,m_num_iocp_resources(0)
    #endif
{
    EventLoop::AsyncSignalList::initLonely(m_pending_async_list);
    EventLoop::AsyncSignalList::initLonely(m_dispatch_async_list);    
}

EventLoop::EventLoop () :
    EventLoopMembers(),
    EventProvider()
{}

EventLoop::~EventLoop ()
{
    AIPSTACK_ASSERT(m_num_timers == 0)
    AIPSTACK_ASSERT(m_timer_heap.isEmpty())
    AIPSTACK_ASSERT(m_num_async_signals == 0)
    AIPSTACK_ASSERT(AsyncSignalList::isLonely(m_pending_async_list))
    AIPSTACK_ASSERT(AsyncSignalList::isLonely(m_dispatch_async_list))
    #if AIPSTACK_EVENT_LOOP_HAS_FD
    AIPSTACK_ASSERT(m_num_fd_notifiers == 0)
    #endif
    #if AIPSTACK_EVENT_LOOP_HAS_IOCP
    AIPSTACK_ASSERT(m_num_iocp_notifiers == 0)
    #endif
    
    #if AIPSTACK_EVENT_LOOP_HAS_IOCP
    try {
        wait_for_final_iocp_results();
    } catch (std::runtime_error const &ex) {
        // Should not happen. Here we leak IocpResource's including user_resource's.
        std::fprintf(stderr, "EventLoop: exception in wait_for_final_iocp_results "
            "(memory leaked): %s\n", ex.what());
    }
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

        if (AIPSTACK_UNLIKELY(m_recheck_async_signals)) {
            if (!dispatch_async_signals()) {
                return;
            }
        }

        if (!EventProvider::dispatchEvents()) {
            return;
        }

        EventLoopTime wait_time = get_timers_wait_time();

        EventProvider::waitForEvents(wait_time);
    }
}

void EventLoop::prepare_timers_for_dispatch (EventLoopTime now)
{
    bool changed = false;

    // Find all Pending timers which are expired with respect to 'now' and change their
    // state to Dispatch.
    m_timer_heap.findAllLesserOrEqual(now, [&](EventLoopTimer *tim) {
        AIPSTACK_ASSERT(tim->m_state == OneOfHeapTimerStates())

        if (tim->m_state == TimerState::Pending) {
            tim->m_state = TimerState::Dispatch;
            changed = true;
        }
    });

    // It is important to understand that the changes performed (taken together) must not
    // break the structure of the heap. Specifically, given any two timers, their relative
    // order must remain the same or they become equal. This is satisfied because all
    // Dispatch state timers compare equal among themselves and less than any Pending
    // timer.

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

        m_timer_heap.remove(*tim);
        tim->m_state = TimerState::Idle;

        tim->m_handler();

        if (AIPSTACK_UNLIKELY(m_stop)) {
            return false;
        }
    }

    return true;
}

EventLoopTime EventLoop::get_timers_wait_time () const
{
    EventLoopTimer *tim = m_timer_heap.first();
    if (tim == nullptr) {
        return EventLoopTime::max();
    }
    AIPSTACK_ASSERT(tim->m_state == TimerState::Pending)
    return tim->m_time;
}

bool EventLoop::dispatch_async_signals ()
{
    // This flag is used to prevent the possibility of forgetting to dispatch a pending
    // signal in case an exception occurs during dispatching below and run() is called
    // again afterward. If this occurs, any next run() would see this flag to be true and
    // call this function before waitForEvents.
    m_recheck_async_signals = true;

    {
        std::unique_lock<std::mutex> lock(m_async_signal_mutex);

        // Move any signals in the pending list to the end of the dispatch list.
        if (!AsyncSignalList::isLonely(m_pending_async_list)) {
            AsyncSignalList::moveOtherNodesBefore(
                m_pending_async_list, m_dispatch_async_list);
        }

        // Dispatch signals in the dispatch list.
        while (true) {
            // Get the next signal, if any (note the list is circular).
            AsyncSignalNode *node = AsyncSignalList::next(m_dispatch_async_list);
            if (node == &m_dispatch_async_list) {
                break;
            }

            EventLoopAsyncSignal &asig = *static_cast<EventLoopAsyncSignal *>(node);
            AIPSTACK_ASSERT(&asig.m_loop == this)
            AIPSTACK_ASSERT(!AsyncSignalList::isRemoved(asig))

            // Remove the signal from the list.
            AsyncSignalList::remove(asig);
            AsyncSignalList::markRemoved(asig);

            // Unlock the mutex while calling the handler.
            lock.unlock();

            asig.m_handler();

            if (AIPSTACK_UNLIKELY(m_stop)) {
                return false;
            }

            // Lock mutex again before looking at the dispatch list.
            lock.lock();
        }
    }

    // No exception occurred, clear this flag.
    m_recheck_async_signals = false;

    return true;
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

EventLoopTimer::EventLoopTimer (EventLoop &loop, TimerHandler handler) :
    m_loop(loop),
    m_handler(handler),
    m_time(EventLoopTime()),
    m_state(TimerState::Idle)
{
    m_loop.m_num_timers++;
}

EventLoopTimer::~EventLoopTimer ()
{
    if (m_state != TimerState::Idle) {
        m_loop.m_timer_heap.remove(*this);
    }

    AIPSTACK_ASSERT(m_loop.m_num_timers > 0)
    m_loop.m_num_timers--;
}

void EventLoopTimer::unset ()
{
    if (m_state != TimerState::Idle) {
        m_loop.m_timer_heap.remove(*this);
        m_state = TimerState::Idle;
    }
}

void EventLoopTimer::setAt (EventLoopTime time)
{
    m_time = time;

    TimerState old_state = m_state;
    m_state = TimerState::Pending;

    if (old_state == TimerState::Idle) {
        m_loop.m_timer_heap.insert(*this);
    } else {
        m_loop.m_timer_heap.fixup(*this);            
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
{
    m_loop.m_num_fd_notifiers++;
}

EventLoopFdWatcher::~EventLoopFdWatcher ()
{
    if (m_watched_fd >= 0) {
        EventProviderFd::resetImpl();
    }

    AIPSTACK_ASSERT(m_loop.m_num_fd_notifiers > 0)
    m_loop.m_num_fd_notifiers--;
}

void EventLoopFdWatcher::initFd (int fd, EventLoopFdEvents events)
{
    AIPSTACK_ASSERT(m_watched_fd == -1)
    AIPSTACK_ASSERT(fd >= 0)
    AIPSTACK_ASSERT((events & ~EventLoopFdEvents::All) == EnumZero)

    EventProviderFd::initFdImpl(fd, events);

    // Update these after initFdImpl so they remain unchanged in case of exception.
    m_watched_fd = fd;
    m_events = events;
}

void EventLoopFdWatcher::updateEvents (EventLoopFdEvents events)
{
    AIPSTACK_ASSERT(m_watched_fd >= 0)
    AIPSTACK_ASSERT((events & ~EventLoopFdEvents::All) == EnumZero)

    EventProviderFd::updateEventsImpl(events);

    // Update these after updateEventsImpl so they remain unchanged in case of exception.
    m_events = events;
}

void EventLoopFdWatcher::reset ()
{
    if (m_watched_fd >= 0) {
        EventProviderFd::resetImpl();

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

int EventProviderFdBase::getFd () const
{
    auto &fd_watcher = static_cast<EventLoopFdWatcher const &>(*this);
    return fd_watcher.m_watched_fd;
}

EventLoopFdEvents EventProviderFdBase::getFdEvents () const
{
    auto &fd_watcher = static_cast<EventLoopFdWatcher const &>(*this);
    return fd_watcher.m_events;
}

bool EventProviderFdBase::callFdEventHandler (EventLoopFdEvents events)
{
    auto &fd_watcher = static_cast<EventLoopFdWatcher &>(*this);

    fd_watcher.m_handler(events);

    if (AIPSTACK_UNLIKELY(fd_watcher.m_loop.m_stop)) {
        return false;
    }

    return true;
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
        // Call waitForEvents only on non-first iterations, after having just called
        // dispatchEvents. This is because we must not call waitForEvents before all
        // available events have been dispatched.
        if (!first_try) {
            EventProvider::waitForEvents(EventLoopTime::max());
        }
        first_try = false;

        // Call dispatchEvents to wait for IOCP operations to complete.
        bool dispatch_res = EventProvider::dispatchEvents();

        // dispatchEvents only returns false if it observed m_stop after having called
        // an event handler. This cannot happen here because there are no event handlers
        // that could be called.
        AIPSTACK_ASSERT(dispatch_res)
    }
}

#endif

EventLoopAsyncSignal::EventLoopAsyncSignal (EventLoop &loop, SignalEventHandler handler) :
    m_loop(loop),
    m_handler(handler)
{
    AsyncSignalList::markRemoved(*this);

    m_loop.m_num_async_signals++;
}

EventLoopAsyncSignal::~EventLoopAsyncSignal ()
{
    reset();

    AIPSTACK_ASSERT(m_loop.m_num_async_signals > 0)
    m_loop.m_num_async_signals--;
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
