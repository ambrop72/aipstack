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

/**
 * @addtogroup event-loop
 * @{
 */

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
    std::mutex m_async_signal_mutex;
    EventLoopPriv::AsyncSignalNode m_pending_async_list;
    EventLoopPriv::AsyncSignalNode m_dispatch_async_list;
    #if AIPSTACK_EVENT_LOOP_HAS_IOCP
    std::size_t m_num_iocp_notifiers;
    std::size_t m_num_iocp_resources;
    #endif
};
#endif

/**
 * Represents a single event loop.
 * 
 * For details about the event loop implementation, see @ref event-loop.
 * 
 * An event loop is used by constructing it, initializing related objects which provide
 * notifications (such as @ref EventLoopTimer), then calling the @ref run function. From
 * within the @ref run function, the event loop calls event handlers as associated events
 * are detected. The event loop can be stopped using the @ref stop function.
 * 
 * Event handlers are generally free to interact with the event loop and related objects
 * (including construct and destruct objects), with the exception that they must not
 * destruct the event loop.
 * 
 * The event loop is not thread-safe; a single @ref EventLoop instance and all objects
 * which use that event loop may only be used in the context of a single thread except
 * where documented otherwise.
 */
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
    /**
     * Construct the event loop.
     * 
     * @throw std::runtime_error If an error occurs in platform-specific initialization of
     *        event facilities.
     * @throw std::bad_alloc If a memory allocation error occurs.
     */
    EventLoop ();

    /**
     * Destruct the event loop.
     * 
     * The event loop must not be destructed from within the @ref run function (that is
     * from within event handlers).
     */
    ~EventLoop ();

    /**
     * Stop the event loop by making the @ref run function return at the earliest
     * oppurtunity.
     * 
     * If stop is called from outside of @ref run, then any subsequent @ref run call will
     * return immediately without calling any event handlers. If stop is called from within
     * @ref run, then @ref run will return after the current event handler returns.
     */
    void stop ();

    /**
     * Run the event loop, calling event handlers as events are detected.
     * 
     * Note that after @ref stop was called, further calls of @ref run will simply return
     * immediately.
     * 
     * @throw std::runtime_error If an unexpected error occurs. It is currently not safe
     *        to call @ref run again after an exception.
     * @throw any Any exceptions from event handlers are propagated directly.
     */
    void run ();

    /**
     * Get the current time from the clock used by the event loop (@ref EventLoopClock).
     * 
     * This is equivalent to calling `EventLoopClock::now()`.
     * 
     * @return Current time from @ref EventLoopClock.
     */
    inline static EventLoopTime getTime ()
    {
        return EventLoopClock::now();
    }

    /**
     * Get the current cached event time.
     * 
     * This returns the time cached by the event loop. The returned time is guaranteed to
     * have been obtained from the @ref EventLoopClock at some point in the past. It is
     * preferrable to use this instead of @ref getTime for performance reasons.
     * 
     * The cached event time is updated in the following scenarios:
     * - When the event loop is constructed.
     * - Just before the event loop dispatches a batch of events.
     * 
     * @return Cached event time.
     */
    inline EventLoopTime getEventTime () const
    {
        return m_event_time;
    }

    #if AIPSTACK_EVENT_LOOP_HAS_IOCP || defined(IN_DOXYGEN)
    /**
     * Call `CreateIoCompletionPort` to register a handle with the IOCP handle used by
     * the event loop (Windows only).
     * 
     * It is necessary to call this function in order to receive IOCP events from
     * application-managed IOCP-capable handles (combined with the use of @ref
     * EventLoopIocpNotifier). The association is done using an unspecified completion key
     * determined and used by the event loop implementation.
     * 
     * Note that after a handle is associated, is is imperative that any asynchronous I/O
     * operations performed with that handle are done with the assistance of @ref
     * EventLoopIocpNotifier as described in the documentation for that class. If this is
     * not respected, assertion errors and/or crashes will result as the event loop would
     * receive an unexpected IOCP event.
     * 
     * @param handle Handle to associate with IOCP.
     * @param out_error If association fails, the Windows error code resulting from
     *        `CreateIoCompletionPort` will be stored here (unchanged on success).
     * @return True on success, false on failure.
     */
    bool addHandleToIocp (HANDLE handle, DWORD &out_error);
    #endif

private:
    void prepare_timers_for_dispatch (EventLoopTime now);

    bool dispatch_timers ();

    EventLoopTime prepare_timers_for_wait ();

    bool dispatch_async_signals ();

    #if AIPSTACK_EVENT_LOOP_HAS_IOCP
    bool handle_iocp_result (void *completion_key, OVERLAPPED *overlapped);
    void wait_for_final_iocp_results ();
    #endif
};

/**
 * Provides scheduled notifications based on the @ref EventLoopClock.
 * 
 * This class allows requesting notification when a specific clock time is reached
 * by the @ref EventLoopClock (the choice of this clock depends on the platform).
 * 
 * A timer object is always in one of two states, running and stopped. The timer is
 * started by calling @ref setAt or @ref setAfter and can be manually stopped by calling
 * @ref unset. The state of a timer can be queried using @ref isSet.
 * 
 * A running timer has an asssociated expiration time (which can be queried using @ref
 * getSetTime). Soon after the expiration time of a running timer is reached, the @ref
 * handleTimerExpired callback will be called, and the timer will transition to stopped
 * state just prior to the call.
 * 
 * In order to use the timer, one must define a derived class and override the @ref
 * handleTimerExpired pure virtual function.
 */
class EventLoopTimer :
    private NonCopyable<EventLoopTimer>
{
    friend class EventLoopPriv;
    friend class EventLoop;

    AIPSTACK_USE_TYPES(EventLoop, (TimerHeapNode, TimerState))

public:
    /**
     * Construct the timer; the timer is initially stopped.
     * 
     * @param loop Event loop; it must outlive the timer object.
     */
    EventLoopTimer (EventLoop &loop);

    /**
     * Destruct the timer.
     */
    ~EventLoopTimer ();

    /**
     * Get the running state of the timer.
     * 
     * @return True if the timer is running, false if it is stopped.
     */
    inline bool isSet () const
    {
        return m_state != OneOf(TimerState::Idle, TimerState::TempUnset);
    }

    /**
     * Get the last expiration time of the timer.
     * 
     * @return If the timer is running, its current expiration time. If the timer is not
     *         running, its last set expiration time or zero if it has never been started.
     */
    inline EventLoopTime getSetTime () const
    {
        return m_time;
    }

    /**
     * Stop the timer.
     * 
     * The timer enters to stopped state (if that was not the case already) and the @ref
     * handleTimerExpired callback will not be called before the timer is started next.
     */
    void unset ();

    /**
     * Start the timer to expire at the given (absolute) time.
     * 
     * The timer enters running state and its expiration time is set to the given time.
     * Note that it is valid to set an expiration time in the past, which would result in
     * the callback being Call soon.
     * 
     * @param time Expiration time.
     */
    void setAt (EventLoopTime time);

    /**
     * Start the timer to expire after the given (relative) duration.
     * 
     * This function is equivalent to calling
     * @ref setAt(@ref EventLoop::getEventTime()+duration).
     * 
     * @param duration Duration to expire after, relative to
     *        @ref EventLoop::getEventTime().
     */
    void setAfter (EventLoopDuration duration);

protected:
    /**
     * Callback function called after the timer has expired.
     * 
     * It is guaranteed that the timer was in running state just before the call and
     * that the scheduled expiration time has been reached. The timer transitions to
     * stopped state just before the call.
     * 
     * The callback is always called asynchronously (not from any public member function).
     */
    virtual void handleTimerExpired () = 0;

private:
    TimerHeapNode m_heap_node;
    EventLoop &m_loop;
    EventLoopTime m_time;
    TimerState m_state;
};

/**
 * Invokes a callback in the event loop after a signal from an arbitrary thread.
 * 
 * An async-signal object provides a mechanism for arbitrary threads to generate events
 * (callbacks) in the event loop.
 * 
 * The essential function of this class is a guarantee that that the @ref
 * SignalEventHandler callback will be called in the event loop soon after @ref signal
 * is called, where it specifically allowed to call @ref signal from any thread. No
 * transfer of data is provided and multiple subsequent @ref signal calls may result in
 * only one callback.
 */
class EventLoopAsyncSignal :
    private NonCopyable<EventLoopAsyncSignal>
    #ifndef IN_DOXYGEN
    ,private EventLoop::AsyncSignalNode
    #endif
{
    friend class EventLoop;

    AIPSTACK_USE_TYPES(EventLoop, (AsyncSignalList))

public:
    /**
     * Type of callback function used to report previous @ref signal calls.
     * 
     * See the @ref EventLoopAsyncSignal class description for details.
     * 
     * The callback is always called asynchronously (not from any public member function).
     */
    using SignalEventHandler = Function<void()>;

    /**
     * Construct the async-signal object.
     * 
     * It is guaranteed that the @ref SignalEventHandler callback will not be called until
     * the first @ref signal call.
     * 
     * @param loop Event loop; it must outlive the async-signal object.
     * @param handler Callback function (must not be null).
     */
    EventLoopAsyncSignal (EventLoop &loop, SignalEventHandler handler);

    /**
     * Destruct the async-signal object.
     * 
     * The @ref SignalEventHandler callback will not be called after destruction.
     * 
     * @note It is clearly the responsibility of the application to not call @ref signal on
     * a destructed async-signal object or one which may be destructed during the call.
     */
    ~EventLoopAsyncSignal ();

    /**
     * Request that the @ref SignalEventHandler be called in the event loop soon.
     * 
     * This function is specifically thread-safe (unlike other functions which are not,
     * by default). However be careful with destruction of the async-signal object (see the
     * note in the destructor).
     */
    void signal ();

    /**
     * Reset the async-signal object to its initial state by clearing any pending callback
     * request.
     * 
     * Calling this function guarantees that the @ref SignalEventHandler callback will not
     * be called until the next @ref signal call. This function should be unnecessary as
     * long as the implementation of the @ref SignalEventHandler callback by the
     * application checks that requisite conditions are satisified instead of assuming that
     * a callback implies a previous call to @ref signal.
     */
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

/**
 * Provides notifications about I/O readiness of a file descriptor (Linux only).
 * 
 * An fd-watcher provide notifications when an application-specified file descriptor is
 * ready for a certain type of I/O operation.
 * 
 * In order to monitor a file descriptor, the @ref initFd function should be called
 * after construction to specify the file descritor and types of I/O readiness to
 * monitor for. These types can subsequently be modified using @ref updateEvents.
 */
class EventLoopFdWatcher :
    private NonCopyable<EventLoopFdWatcher>
    #ifndef IN_DOXYGEN
    ,private EventLoopFdWatcherMembers,
    private EventProviderFd
    #endif
{
    friend class EventProviderFdBase;

public:
    /**
     * Type of callback used to report file descriptor events.
     * 
     * Note that the @ref EventLoopFdEvents::Error and @ref EventLoopFdEvents::Hup events
     * may be reported even if they were not requested; see @ref EventLoopFdEvents for
     * details and justifications.
     * 
     * The callback is always called asynchronously (not from any public member function).
     * 
     * @param events Set of reported events (guaranteed to be non-empty).
     */
    using FdEventHandler = Function<void(EventLoopFdEvents events)>;

    /**
     * Construct the fd-watcher object.
     * 
     * Initially, the fd-watcher object is not monitoring any file descriptor. Associate
     * the object with a file descriptor using @ref initFd.
     * 
     * @param loop Event loop; it must outlive the fd-watcher object.
     * @param handler Callback function (must not be null).
     * @throw std::runtime_error If an error occurs in platform-specific setup of
     *        file-descriptor monitoring.
     * @throw std::bad_alloc If a memory allocation error occurs.
     */
    EventLoopFdWatcher (EventLoop &loop, FdEventHandler handler);

    /**
     * Destruct the fd-watcher object.
     * 
     * @warning Be careful with the declaration order of members in your classes to ensure
     * that the an fd-watcher is destructed before its monitored file descriptor is closed
     * (e.g. @ref FileDescriptorWrapper should come before @ref EventLoopFdWatcher).
     */
    ~EventLoopFdWatcher ();

    /**
     * Determine whether the fd-watcher object is monitoring a file descriptor.
     * 
     * @return True if monitoring a file descriptor, false if not.
     */
    inline bool hasFd () const
    {
        return m_watched_fd >= 0;
    }

    /**
     * Get the monitored file descriptor.
     * 
     * @return The monitored file descritor, or -1 if not monitoring.
     */
    inline int getFd () const
    {
        return m_watched_fd;
    }

    /**
     * Get the types of I/O readiness being monitored.
     * 
     * @return Types of I/O readiness being monitored, none if not monitoring.
     */
    inline EventLoopFdEvents getEvents () const
    {
        return m_events;
    }

    /**
     * Start monitoring a file descriptor.
     * 
     * @note This function must not be called if a file descriptor is already being
     * monitored. Either first call @ref reset, or use @ref updateEvents if you only need
     * to change the types of I/O readiness being monitored for.
     * 
     * @warning Do not close the file descriptor while it is being monitored. Call @ref
     * reset or destruct the fs-watcher object before closing the file descriptor. See also
     * the related note in the destructor. Violating this requirement might not cause
     * problems with Linux/epoll but might on other operating systems and event providers
     * that may be supported in the future.
     * 
     * @param fd File descriptor.
     * @param events Mask of I/O readiness types to monitor for, see @ref
     *        EventLoopFdEvents. Only bits which are defined there may be included.
     * @throw std::runtime_error If an error occurs registering the file descriptor with
     *        the event notification system (e.g. epoll).
     */
    void initFd (int fd, EventLoopFdEvents events = {});

    /**
     * Set the types of I/O readiness that the file descriptor is being monitored for.
     * 
     * @param events Mask of I/O readiness types to monitor for, see @ref
     *        EventLoopFdEvents. Only bits which are defined there may be included.
     * @throw std::runtime_error If an error occurs changing the I/O types for the file
     *        descriptor in the event notification system (e.g. epoll).
     */
    void updateEvents (EventLoopFdEvents events);

    /**
     * Reset the fd-watcher object to the initial state where it is not monitoring any file
     * descriptor.
     */
    void reset ();
};

#endif

#if AIPSTACK_EVENT_LOOP_HAS_IOCP || defined(IN_DOXYGEN)

/**
 * Provides notifications of completed IOCP operations (Windows only).
 * 
 * An IOCP-notifier object contains an `OVERLAPPED` structure and is able to report
 * completion of asynchronous I/O operations which have been started using that
 * `OVERLAPPED` structure.
 * 
 * An IOCP-notifier object has three states:
 * - Unprepared (this is the default after construction). In this state, resources required
 *   to deal with an I/O operation have not been allocated and it is not allowed to call
 *   @ref ioStarted. Call @ref prepare to get to Idle state.
 * - Idle. In this state, the object is ready to become responsible for an I/O operation.
 *   Call @ref ioStarted as soon as an operation is started successfully, which will cause
 *   a transition to the Busy state.
 * - Busy. In this state, the object is responsible for an ongoing I/O operation. The
 *   @ref EventLoopIocpNotifier callback will be called when the operation completes.
 * 
 * Concurrent operations are not allowed with the same object, but mutliple IOCP-notifier
 * instances can be used with the same handle if concurrent operations are needed.
 * 
 * The IOCP-capable handle on which asynchronous I/O is to be performed must first be
 * associated with the IOCP instance managed by the event loop, using @ref
 * EventLoop::addHandleToIocp (which is a wrapper around `CreateIoCompletionPort`). This
 * should be done only once for a handle.
 * 
 * Once the handle is associated, any asynchronous I/O operation on that handle must be
 * performed as follows:
 * - Obtain an IOCP-notifier object in Idle state to be used for this operation.
 * - Initialize the `OVERLAPPED` structure of the IOCP-notifier object as appropriate
 *   for the operation (possibly simply zero it); use @ref getOverlapped to access the
 *   structure.
 * - Start the asynchronous I/O operation using the appropriate Windows API function
 *   while passing the pointer to the same `OVERLAPPED` structure. If that fails do not
 *   proceed since there would be no completion event.
 * - Call @ref ioStarted. This is essential, if you fail to do this the event loop may
 *   receive the completion event when it is not expecting it, and undefined behavior will
 *   occur.
 * - The @ref IocpEventHandler callback function will be called when the operation has
 *   completed. From this callback you will want to call `GetOverlappedResult` to retrieve
 *   the result of the operation.
 * 
 * An IOCP-notifier object can be destructed or @ref reset even in Busy state, in which
 * case the implementation will still expect and be able to handle the completion message.
 * Further, the application may specify an abstract resource (possibly containing the
 * buffer used for the operation) which is to be kept alive until the operation completes;
 * see @ref ioStarted for details.
 */
class EventLoopIocpNotifier :
    private NonCopyable<EventLoopIocpNotifier>
{
    friend class EventLoop;

    AIPSTACK_USE_TYPES(EventLoop, (IocpResource))

public:
    /**
     * Type of callback used to report completion of an asynchronous I/O operation.
     * 
     * It is guaranteed that the IOCP-notifier object was in Busy state just before the
     * call. The object transitions to Idle state just before the call.
     * 
     * The callback is always called asynchronously (not from any public member function).
     */
    using IocpEventHandler = Function<void()>;

    /**
     * Construct the IOCP-notifier object.
     * 
     * The object is initially in Unprepared state; @ref prepare must be called before
     * @ref ioStarted.
     * 
     * @param loop Event loop; it must outlive the IOCP-notifier object.
     * @param handler Callback function (must not be null).
     */
    EventLoopIocpNotifier (EventLoop &loop, IocpEventHandler handler);

    /**
     * Destruct the IOCP-notifier object.
     * 
     * See the notes about destruction in Busy state in @ref ioStarted.
     */
    ~EventLoopIocpNotifier ();

    /**
     * Allocate resources to allow the IOCP-notifier object to handle an asynchronous
     * I/O operation.
     * 
     * This may be called only in Unprepared state. On success (no exception), it the
     * object transitions to Idle state.
     * 
     * @note The reason for the existence of the Unprepared state is that a block of
     * dynamically allocated memory (which includes `OVERLAPPED`) has to be kept alive if
     * the IOCP-notifier object is destructed or @ref reset in Busy state, until the I/O
     * operation completes. Without this state, @ref reset may have to allocate a new block
     * of memory (while keeping the old one alive) and therefore could throw, which does
     * not seem right.
     * 
     * @throw std::bad_alloc If a memory allocation error occurs. The object did not
     *        transition to Idle state).
     */
    void prepare ();

    /**
     * Reset the IOCP-notifier object bringing it to Unprepared state.
     * 
     * This is equivalent to destructing and reconstrucing the object. The notes in @ref
     * ioStarted concerning destruction in Busy state apply.
     */
    void reset ();

    /**
     * Indicate that an asynchronous I/O operation has started and optionally specify a
     * resource to be kept alive while the operation is running.
     * 
     * This may only be called in Idle state.
     * 
     * This function accepts a `shared_ptr` representing an opaque resource to which a
     * reference (`shared_ptr` instance) will be kept for the duration of the I/O
     * operation. If completion of the I/O operation is reported by the @ref
     * IocpEventHandler callback, the reference is released just before the callback. On
     * the other hand, if the operation is abandoned before the callback (by destructing
     * this object or calling @ref reset), the reference would be released later. Note that
     * the @ref EventLoop destructor will wait for any such outstanding operations to
     * complete, therefore the reference may also be released from there as opposed to from
     * @ref EventLoop::run.
     * 
     * @param user_resource Shared pointer to an opaque resource to which a reference will
     *        be kept for the duration of the I/O operation (may be null).
     */
    void ioStarted (std::shared_ptr<void> user_resource);

    /**
     * Return whether the IOCP-notifier object has been prepared.
     * 
     * @return True if the object is in Idle or Busy state, false if in Unprepared state.
     */
    inline bool isPrepared () const { return m_iocp_resource != nullptr; }

    /**
     * Return whether the IOCP-notifier object is in Busy state.
     * 
     * @return The if the object is in Busy state, false if in Unprepared or Idle state.
     */
    inline bool isBusy () const { return m_busy; }

    /**
     * Return a reference to the `OVERLAPPED` structure managed by this object.
     * 
     * This must not be called in Unprepared state.
     * 
     * @return Reference to the `OVERLAPPED` structure.
     */
    OVERLAPPED & getOverlapped ();

private:
    EventLoop &m_loop;
    Function<void()> m_handler;
    IocpResource *m_iocp_resource;
    bool m_busy;
};

#endif

/** @} */

}

#endif
