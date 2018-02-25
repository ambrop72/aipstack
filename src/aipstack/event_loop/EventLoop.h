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
#include <mutex>

#include <aipstack/misc/NonCopyable.h>
#include <aipstack/misc/OneOf.h>
#include <aipstack/misc/Use.h>
#include <aipstack/misc/Function.h>
#include <aipstack/structure/LinkModel.h>
#include <aipstack/structure/StructureRaiiWrapper.h>
#include <aipstack/structure/LinkedList.h>
#include <aipstack/structure/minimum/LinkedHeap.h>
#include <aipstack/event_loop/EventLoopCommon.h>

#if defined(__linux__)
#include <aipstack/event_loop/platform_specific/EventProviderLinux.h>
#elif defined(_WIN32)
#include <aipstack/event_loop/platform_specific/EventProviderWindows.h>
#else
#error "Unsupported OS"
#endif

#if AIPSTACK_EVENT_LOOP_HAS_IOCP
#include <cstddef>
#include <memory>
#include <windows.h>
#endif

namespace AIpStack {

#ifndef IN_DOXYGEN
struct EventLoopMembers;
class EventLoop;
class EventLoopTimer;
class EventLoopAsyncSignal;
#if AIPSTACK_EVENT_LOOP_HAS_IOCP
class EventLoopIocpNotifier;
#endif
#endif

#ifndef IN_DOXYGEN
class EventLoopPriv {
    friend struct EventLoopMembers;
    friend class EventLoop;

    struct TimerHeapNodeAccessor;
    struct TimerCompare;

    using TimerLinkModel = PointerLinkModel<EventLoopTimer>;
    using TimerHeap = LinkedHeap<TimerHeapNodeAccessor, TimerCompare, TimerLinkModel>;
    using TimerHeapNode = LinkedHeapNode<TimerLinkModel>;

    struct AsyncSignalNode;
    struct AsyncSignalNodeAccessor;

    using AsyncSignalLinkModel = PointerLinkModel<AsyncSignalNode>;
    using AsyncSignalList = CircularLinkedList<
        AsyncSignalNodeAccessor, AsyncSignalLinkModel>;
    using AsyncSignalListNode = LinkedListNode<AsyncSignalLinkModel>;

    struct AsyncSignalNode {
        AsyncSignalListNode m_list_node;
    };

    #if AIPSTACK_EVENT_LOOP_HAS_IOCP
    struct IocpResource {
        // The overlapped must be the first field so that we can easily convert
        // from OVERLAPPED* to IocpResource*.
        OVERLAPPED overlapped;
        EventLoop *loop;
        EventLoopIocpNotifier *notifier;
        std::shared_ptr<void> user_resource;
    };
    #endif
};
#endif

#ifndef IN_DOXYGEN
struct EventLoopMembers {
    EventLoopMembers();
    
    StructureRaiiWrapper<EventLoopPriv::TimerHeap> m_timer_heap;
    bool m_stop;
    EventLoopTime m_event_time;
    EventLoopTime m_last_wait_time;
    std::mutex m_async_signal_mutex;
    EventLoopPriv::AsyncSignalNode m_pending_async_list;
    EventLoopPriv::AsyncSignalNode m_dispatch_async_list;
    #if AIPSTACK_EVENT_LOOP_HAS_IOCP
    std::size_t m_num_iocp_notifiers;
    std::size_t m_num_iocp_resources;
    #endif
};
#endif

class EventLoop :
    private NonCopyable<EventLoop>
    #ifndef IN_DOXYGEN
    ,private EventLoopMembers,
    private EventProvider
    #endif
{
    friend class EventProviderBase;
    friend class EventLoopPriv;
    friend struct EventLoopMembers;
    friend class EventLoopTimer;
    friend class EventLoopAsyncSignal;
    #if AIPSTACK_EVENT_LOOP_HAS_FD
    friend class EventProviderFdBase;
    #endif
    #if AIPSTACK_EVENT_LOOP_HAS_IOCP
    friend class EventLoopIocpNotifier;
    #endif

    AIPSTACK_USE_TYPES(EventLoopPriv, (TimerHeapNode))

    static int const TimerStateOrderBits = 2;
    static std::uint8_t const TimerStateOrderMask = (1 << TimerStateOrderBits) - 1;

    enum class TimerState : std::uint8_t {
        Idle       = 0,
        Dispatch   = 1,
        TempUnset  = 2,
        TempSet    = 2 | (1 << TimerStateOrderBits),
        Pending    = 3
    };

    static constexpr auto OneOfHeapTimerStates ()
    {
        return OneOf(TimerState::Dispatch, TimerState::TempUnset,
                     TimerState::TempSet, TimerState::Pending);
    }

    AIPSTACK_USE_TYPES(EventLoopPriv, (AsyncSignalNode, AsyncSignalList))

    #if AIPSTACK_EVENT_LOOP_HAS_IOCP
    AIPSTACK_USE_TYPES(EventLoopPriv, (IocpResource))
    #endif

public:
    EventLoop ();

    ~EventLoop ();

    void stop ();

    void run ();

    inline static EventLoopTime getTime ()
    {
        return EventLoopClock::now();
    }

    inline EventLoopTime getEventTime () const
    {
        return m_event_time;
    }

private:
    void prepare_timers_for_dispatch (EventLoopTime now);

    bool dispatch_timers ();

    EventLoopWaitTimeoutInfo prepare_timers_for_wait ();

    EventLoopWaitTimeoutInfo update_last_wait_time (EventLoopTime wait_time);

    bool dispatch_async_signals ();

    #if AIPSTACK_EVENT_LOOP_HAS_IOCP
    bool handle_iocp_result (void *completion_key, OVERLAPPED *overlapped);
    void wait_for_final_iocp_results ();
    #endif
};

class EventLoopTimer :
    private NonCopyable<EventLoopTimer>
{
    friend class EventLoopPriv;
    friend class EventLoop;

    AIPSTACK_USE_TYPES(EventLoop, (TimerHeapNode, TimerState))

public:
    EventLoopTimer (EventLoop &loop);

    ~EventLoopTimer ();

    inline bool isSet () const
    {
        return m_state != OneOf(TimerState::Idle, TimerState::TempUnset);
    }

    inline EventLoopTime getSetTime () const
    {
        return m_time;
    }

    void unset ();

    void setAt (EventLoopTime time);

    void setAfter (EventLoopDuration duration);

protected:
    virtual void handleTimerExpired () = 0;

private:
    TimerHeapNode m_heap_node;
    EventLoop &m_loop;
    EventLoopTime m_time;
    TimerState m_state;
};

class EventLoopAsyncSignal :
    private NonCopyable<EventLoopAsyncSignal>
    #ifndef IN_DOXYGEN
    ,private EventLoop::AsyncSignalNode
    #endif
{
    friend class EventLoop;

    AIPSTACK_USE_TYPES(EventLoop, (AsyncSignalList))

public:
    using SignalEventHandler = Function<void()>;

    EventLoopAsyncSignal (EventLoop &loop, SignalEventHandler handler);

    ~EventLoopAsyncSignal ();

    void signal ();

    void reset ();

private:
    EventLoop &m_loop;
    SignalEventHandler m_handler;
};

#if AIPSTACK_EVENT_LOOP_HAS_FD || defined(IN_DOXYGEN)

#ifndef IN_DOXYGEN
struct EventLoopFdWatcherMembers {
    EventLoop &m_loop;
    Function<void(EventLoopFdEvents)> m_handler;
    int m_watched_fd;
    EventLoopFdEvents m_events;
};
#endif

class EventLoopFdWatcher :
    private NonCopyable<EventLoopFdWatcher>
    #ifndef IN_DOXYGEN
    ,private EventLoopFdWatcherMembers,
    private EventProviderFd
    #endif
{
    friend class EventProviderFdBase;

public:
    using FdEventHandler = Function<void(EventLoopFdEvents events)>;

    EventLoopFdWatcher (EventLoop &loop, FdEventHandler handler);

    ~EventLoopFdWatcher ();

    inline bool hasFd () const
    {
        return m_watched_fd >= 0;
    }

    inline int getFd () const
    {
        return m_watched_fd;
    }

    inline EventLoopFdEvents getEvents () const
    {
        return m_events;
    }

    void initFd (int fd, EventLoopFdEvents events = {});

    void updateEvents (EventLoopFdEvents events);

    void reset ();
};

#endif

#if AIPSTACK_EVENT_LOOP_HAS_IOCP || defined(IN_DOXYGEN)

class EventLoopIocpNotifier :
    private NonCopyable<EventLoopIocpNotifier>
{
    friend class EventLoop;

    AIPSTACK_USE_TYPES(EventLoop, (IocpResource))

public:
    using IocpEventHandler = Function<void()>;

    EventLoopIocpNotifier (EventLoop &loop, IocpEventHandler handler);

    ~EventLoopIocpNotifier ();

    bool associateHandle (HANDLE handle, DWORD &out_error);

    void reset ();

    void ioStarted (std::shared_ptr<void> user_resource);

    inline bool isAssociated () const { return m_iocp_resource != nullptr; }

    inline bool isBusy () const { return m_busy; }

    OVERLAPPED & getOverlapped ();

private:
    EventLoop &m_loop;
    Function<void()> m_handler;
    IocpResource *m_iocp_resource;
    bool m_busy;
};

#endif

}

#endif
