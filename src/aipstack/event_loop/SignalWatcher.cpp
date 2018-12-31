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

#include <stdexcept>

#include <aipstack/misc/Assert.h>
#include <aipstack/event_loop/SignalWatcher.h>

namespace AIpStack {

SignalCollector::SignalCollector (SignalType signals) :
    SignalCollectorMembers{signals, /*m_collector_watcher=*/nullptr},
    SignalCollectorImpl()
{}

SignalCollector::~SignalCollector ()
{
    AIPSTACK_ASSERT(m_collector_watcher == nullptr);
}

SignalWatcherMembers::SignalWatcherMembers (
    EventLoop &loop, SignalCollector &collector, Function<void(SignalInfo)> handler)
:
    m_loop(loop),
    m_collector(collector),
    m_handler(handler)
{
    if (m_collector.m_collector_watcher != nullptr) {
        throw std::logic_error(
            "SignalWatcher: Only one instance may be used with one SignalCollector.");
    }

    m_collector.m_collector_watcher = static_cast<SignalWatcher *>(this);
}

SignalWatcherMembers::~SignalWatcherMembers ()
{
    AIPSTACK_ASSERT(m_collector.m_collector_watcher == static_cast<SignalWatcher *>(this));
    m_collector.m_collector_watcher = nullptr;
}

SignalWatcher::SignalWatcher (
    EventLoop &loop, SignalCollector &collector, SignalHandler handler)
:
    SignalWatcherMembers(loop, collector, handler),
    SignalWatcherImpl()
{}

SignalWatcher::~SignalWatcher ()
{}

SignalType SignalCollectorImplBase::baseGetSignals () const
{
    auto &collector = static_cast<SignalCollector const &>(*this);
    return collector.m_signals;
}

EventLoop & SignalWatcherImplBase::getEventLoop () const
{
    auto &watcher = static_cast<SignalWatcher const &>(*this);
    return watcher.m_loop;
}

SignalCollectorImplBase & SignalWatcherImplBase::getCollector () const
{
    auto &watcher = static_cast<SignalWatcher const &>(*this);
    return watcher.m_collector;
}

void SignalWatcherImplBase::callHandler(SignalInfo signal_info)
{
    auto &watcher = static_cast<SignalWatcher &>(*this);
    return watcher.m_handler(signal_info);
}

}

#include AIPSTACK_SIGNAL_WATCHER_IMPL_IMPL_FILE
