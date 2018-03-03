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

#ifndef AIPSTACK_SIGNAL_WATCHER_IMPL_LINUX_H
#define AIPSTACK_SIGNAL_WATCHER_IMPL_LINUX_H

#include <signal.h>

#include <aipstack/misc/NonCopyable.h>
#include <aipstack/misc/platform_specific/FileDescriptorWrapper.h>
#include <aipstack/event_loop/EventLoop.h>
#include <aipstack/event_loop/SignalWatcherCommon.h>

namespace AIpStack {

class SignalWatcherImplLinux;

class SignalCollectorImplLinux :
    public SignalCollectorImplBase,
    private NonCopyable<SignalCollectorImplLinux>
{
    friend class SignalWatcherImplLinux;
    
public:
    SignalCollectorImplLinux ();

    ~SignalCollectorImplLinux ();

private:
    SignalType m_orig_blocked_signals;
};

class SignalWatcherImplLinux :
    public SignalWatcherImplBase,
    private NonCopyable<SignalWatcherImplLinux>
{
public:
    SignalWatcherImplLinux ();

    ~SignalWatcherImplLinux ();

private:
    inline SignalCollectorImplLinux & getCollector () const;

    void watcherHandler(EventLoopFdEvents events);
    
private:
    // First fd then watcher for proper destruction order.
    FileDescriptorWrapper m_signalfd_fd;
    EventLoopFdWatcher m_watcher;
};

using SignalCollectorImpl = SignalCollectorImplLinux;

using SignalWatcherImpl = SignalWatcherImplLinux;

#define AIPSTACK_SIGNAL_WATCHER_IMPL_IMPL_FILE \
    <aipstack/event_loop/platform_specific/SignalWatcherImplLinux_impl.h>

}

#endif
