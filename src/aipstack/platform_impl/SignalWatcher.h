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
#include <aipstack/platform_impl/EventLoop.h>
#include <aipstack/platform_impl/SignalWatcherCommon.h>
#include <aipstack/platform_impl/SignalBlocker.h>

#if defined(__linux__)
#include <aipstack/platform_specific/SignalWatcherImplLinux.h>
#else
#error "Unsupported OS"
#endif

namespace AIpStack {

class SignalWatcher :
    private NonCopyable<SignalWatcher>,
    private SignalWatcherImpl
{
    friend class SignalWatcherImplBase;
    
public:
    using SignalHandler = Function<void(SignalInfo signal_info)>;

    SignalWatcher (EventLoop &loop, SignalBlocker &blocker, SignalHandler handler);

    ~SignalWatcher ();

    void startWatching (SignalType signals);

    void reset ();

    inline bool isWatching () const {
        return m_watching;
    }

    inline SignalType getWatchedSignals () const {
        return m_watched_signals;
    }

private:
    SignalHandler m_handler;
    bool m_watching;
    SignalType m_watched_signals;
};

}

#endif
