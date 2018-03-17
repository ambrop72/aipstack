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

#ifndef AIPSTACK_SIGNAL_WATCHER_H
#define AIPSTACK_SIGNAL_WATCHER_H

#include <aipstack/misc/NonCopyable.h>
#include <aipstack/misc/Function.h>
#include <aipstack/event_loop/EventLoop.h>
#include <aipstack/event_loop/SignalWatcherCommon.h>

#if defined(__linux__)
#include <aipstack/event_loop/platform_specific/SignalWatcherImplLinux.h>
#elif defined(_WIN32)
#include <aipstack/event_loop/platform_specific/SignalWatcherImplWindows.h>
#else
#error "Unsupported OS"
#endif

namespace AIpStack {

/**
 * @addtogroup event-loop
 * @{
 */

#ifndef IN_DOXYGEN
class SignalWatcherMembers;
class SignalWatcher;
#endif

#ifndef IN_DOXYGEN
struct SignalCollectorMembers {
    SignalType const m_signals;
    SignalWatcher *m_collector_watcher;
};
#endif

/**
 * Enables reception of operating-system signals in the event loop using @ref
 * SignalWatcher.
 * 
 * Signals can be received by first constructing an instance of @ref SignalCollector and
 * then constructing a @ref SignalWatcher referencing the former.
 * 
 * The actual function of @ref SignalCollector is platform-dependent. Currently this is:
 * - On Linux, it blocks the specified signals on construction and unblocks them on
 *   destruction (only if they were not originally blocked). Actual reception of signals
 *   would be done via `signalfd` by @ref SignalWatcher.
 * - On Windows, it sets up a handler using `SetConsoleCtrlHandler` on construction and
 *   removes the handler on destruction. It additionally manages a queue of received
 *   signals for delivery via @ref SignalWatcher.
 * 
 * The level of support for concurrent instances of @ref SignalCollector depends on the
 * platform:
 * - On Linux, concurrent instances are allowed if no signal is collected by more than one
 *   @ref SignalCollector. If this is violated there should not be any exceptions but it
 *   will not work properly for signals watched by more than one instance.
 * - On Windows, concurrent instances are not allowed at all. Attempting to construct a
 *   @ref SignalWatcher while one already exists will result in an exception.
 */
class SignalCollector :
    private NonCopyable<SignalCollector>
    #ifndef IN_DOXYGEN
    ,private SignalCollectorMembers,
    private SignalCollectorImpl
    #endif
{
    friend class SignalCollectorImplBase;
    friend class SignalWatcherImplBase;
    friend class SignalWatcherMembers;

public:
    /**
     * Construct the signal collector.
     * 
     * @param signals Set of signals to collect for future reporting via @ref
     *        SignalWatcher. Signals not supported for the platform are silently ignored.
     * @throw std::runtime_error If an error occurs in platform-specific setup for
     *        collection of signals.
     * @throw std::bad_alloc If a memory allocation error occurs.
     */
    SignalCollector (SignalType signals);

    /**
     * Destruct the signal collector.
     */
    ~SignalCollector ();

    /**
     * Get the set of signals as passed to the constructor.
     * 
     * @return Set of signals.
     */
    inline SignalType getSignals () const {
        return m_signals;
    }
};

#ifndef IN_DOXYGEN
class SignalWatcherMembers {
    friend class SignalWatcherImplBase;
    friend class SignalWatcher;

    SignalWatcherMembers (
        EventLoop &loop, SignalCollector &collector, Function<void(SignalInfo)> handler);

    ~SignalWatcherMembers ();

    EventLoop &m_loop;
    SignalCollector &m_collector;
    Function<void(SignalInfo)> m_handler;
};
#endif

/**
 * Delivers notifications of operating-system signals in combination with @ref
 * SignalCollector.
 * 
 * This class works in the event loop to deliver signal notifications. The set of signals
 * which may be delivered is the same as the referenced @ref SignalCollector was
 * constructed with.
 * 
 * There may be at most one @ref SignalWatcher at a time associated with a single @ref
 * SignalCollector. There is a check in place in the constructor to throw an exception if
 * this would be violated, but the check is not thread-safe. If is only safe to use
 * multiple @ref SignalWatcher instances with the same @ref SignalCollector if at most one
 * could exist at a time; and if instances are owned by different threads appropriate
 * synchronization is in place so that construction of another instance happens after
 * destruction of the previous instance.
 */
class SignalWatcher :
    private NonCopyable<SignalWatcher>
    #ifndef IN_DOXYGEN
    ,private SignalWatcherMembers,
    private SignalWatcherImpl
    #endif
{
    friend class SignalWatcherMembers;
    friend class SignalWatcherImplBase;
    
public:
    /**
     * Type of callback used to deliver a signal.
     * 
     * The callback is always called asynchronously (not from any public member function).
     * 
     * @param signal_info Contains information about the signal; currently just the
     *        @ref SignalType in @ref SignalInfo::type. The signal type will be one of the
     *        signal types that the @ref SignalCollector was constructed with.
     */
    using SignalHandler = Function<void(SignalInfo signal_info)>;

    /**
     * Construct the signal watcher.
     * 
     * @param loop Event loop; it must outlive the signal-watcher object.
     * @param collector Signal collector; it must outlive the signal-watcher object. See
     *        also the class restructions for restrictions regarding use of multiple
     *        instances with the same @ref SignalCollector.
     * @param handler Callback function used to deliver signals (must not be null).
     * @throw std::logic_error If an existing @ref SignalWatcher instance is already
     *        associated with the specified signal-collector.
     * @throw std::runtime_error If an error occurs in platform-specific setup for signal
     *        watching.
     * @throw std::bad_alloc If a memory allocation error occurs.
     */
    SignalWatcher (EventLoop &loop, SignalCollector &collector, SignalHandler handler);

    /**
     * Destruct the signal watcher.
     */
    ~SignalWatcher ();
};

/** @} */

}

#endif
