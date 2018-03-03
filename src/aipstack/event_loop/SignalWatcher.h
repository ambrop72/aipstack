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
    SignalCollector (SignalType signals);

    ~SignalCollector ();

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
    using SignalHandler = Function<void(SignalInfo signal_info)>;

    SignalWatcher (EventLoop &loop, SignalCollector &collector, SignalHandler handler);

    ~SignalWatcher ();
};

}

#endif
