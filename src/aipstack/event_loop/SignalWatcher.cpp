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

#include <aipstack/misc/Assert.h>
#include <aipstack/event_loop/SignalWatcher.h>

namespace AIpStack {

SignalWatcher::SignalWatcher (EventLoop &loop, SignalBlocker &blocker, SignalHandler handler) :
    SignalWatcherImpl(loop, blocker),
    m_handler(handler),
    m_watching(false),
    m_watched_signals(SignalType())
{}

SignalWatcher::~SignalWatcher ()
{
    if (m_watching) {
        SignalWatcherImpl::stop();
    }
}

void SignalWatcher::startWatching (SignalType signals)
{
    AIPSTACK_ASSERT(!m_watching)

    SignalWatcherImpl::start(signals);
    m_watching = true;
    m_watched_signals = signals;
}

void SignalWatcher::reset ()
{
    if (m_watching) {
        SignalWatcherImpl::stop();
        m_watching = false;
        m_watched_signals = SignalType();
    }
}

void SignalWatcherImplBase::callHandler(SignalInfo signal_info)
{
    auto &signal_watcher = static_cast<SignalWatcher &>(*this);
    return signal_watcher.m_handler(signal_info);
}

}

#include AIPSTACK_SIGNAL_WATCHER_IMPL_IMPL_FILE
