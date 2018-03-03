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

#include <cstdio>
#include <mutex>
#include <stdexcept>

#include <windows.h>

#include <aipstack/misc/Function.h>
#include <aipstack/misc/Assert.h>
#include <aipstack/misc/Modulo.h>
#include <aipstack/event_loop/platform_specific/SignalWatcherImplWindows.h>

namespace AIpStack {

static std::mutex signal_collector_global_mutex;
static SignalCollectorImplWindows *signal_collector_instance = nullptr;

SignalCollectorImplWindows::SignalCollectorImplWindows () :
    m_watcher(nullptr),
    m_buffer_start(0),
    m_buffer_length(0)
{
    {
        std::lock_guard<std::mutex> lock(signal_collector_global_mutex);

        if (signal_collector_instance != nullptr) {
            throw std::runtime_error(
                "SignalCollector: Only one instance at a time is allowed.");
        }

        signal_collector_instance = this;
    }

    if (!::SetConsoleCtrlHandler(
        &SignalCollectorImplWindows::consoleCtrlHandlerTrampoline, true))
    {
        {
            std::lock_guard<std::mutex> lock(signal_collector_global_mutex);
            signal_collector_instance = nullptr;
        }
        throw std::runtime_error(
            "SignalCollector: SetConsoleCtrlHandler failed to add handler.");
    }
}

SignalCollectorImplWindows::~SignalCollectorImplWindows ()
{
    if (!::SetConsoleCtrlHandler(
        &SignalCollectorImplWindows::consoleCtrlHandlerTrampoline, false))
    {
        std::fprintf(stderr, "SignalCollector: SetConsoleCtrlHandler failed to "
            "remove handler.\n");
    }

    {
        std::lock_guard<std::mutex> lock(signal_collector_global_mutex);
        AIPSTACK_ASSERT(signal_collector_instance == this)
        signal_collector_instance = nullptr;
    }
}

BOOL SignalCollectorImplWindows::consoleCtrlHandlerTrampoline (DWORD ctrlType)
{
    SignalCollectorImplWindows *inst = signal_collector_instance;
    AIPSTACK_ASSERT(inst != nullptr)

    return inst->consoleCtrlHandler(ctrlType);
}

BOOL SignalCollectorImplWindows::consoleCtrlHandler (DWORD ctrlType)
{
    SignalType sig;

    switch (ctrlType) {
        case CTRL_C_EVENT:
            sig = SignalType::Interrupt;
            break;
        case CTRL_BREAK_EVENT:
            sig = SignalType::Break;
            break;
        case CTRL_CLOSE_EVENT:
            sig = SignalType::Hangup;
            break;
        default:
            sig = SignalType::None;
            break;
    }

    if ((sig & SignalCollectorImplBase::baseGetSignals()) == EnumZero) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_buffer_length < BufferSize) {
            std::size_t write_pos = Modulo(BufferSize).add(m_buffer_start, m_buffer_length);
            m_buffer[write_pos] = sig;
            m_buffer_length++;
        }

        if (m_watcher != nullptr) {
            m_watcher->m_async_signal.signal();
        }
    }

    return true;
}

SignalWatcherImplWindows::SignalWatcherImplWindows () :
    m_async_signal(
        SignalWatcherImplBase::getEventLoop(),
        AIPSTACK_BIND_MEMBER(&SignalWatcherImplWindows::asyncSignalHandler, this))
{
    SignalCollectorImplWindows &collector = getCollector();

    {
        std::lock_guard<std::mutex> lock(collector.m_mutex);
        AIPSTACK_ASSERT(collector.m_watcher == nullptr)
        collector.m_watcher = this;
    }

    m_async_signal.signal();
}

SignalWatcherImplWindows::~SignalWatcherImplWindows ()
{
    SignalCollectorImplWindows &collector = getCollector();

    {
        std::lock_guard<std::mutex> lock(collector.m_mutex);
        AIPSTACK_ASSERT(collector.m_watcher == this)
        collector.m_watcher = nullptr;
    }
}

SignalCollectorImplWindows & SignalWatcherImplWindows::getCollector () const
{
    return static_cast<SignalCollectorImplWindows &>(SignalWatcherImplBase::getCollector());
}

void SignalWatcherImplWindows::asyncSignalHandler ()
{
    SignalCollectorImplWindows &collector = getCollector();

    SignalType sig;
    bool have_more;

    {
        std::lock_guard<std::mutex> lock(collector.m_mutex);

        if (collector.m_buffer_length == 0) {
            return;
        }

        sig = collector.m_buffer[collector.m_buffer_start];
        
        collector.m_buffer_start = Modulo(BufferSize).inc(collector.m_buffer_start);
        collector.m_buffer_length--;

        have_more = (collector.m_buffer_length > 0);
    }

    if (have_more) {
        m_async_signal.signal();
    }

    return SignalWatcherImplBase::callHandler(SignalInfo{sig});
}

}
