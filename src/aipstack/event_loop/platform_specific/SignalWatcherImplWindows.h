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

#ifndef AIPSTACK_SIGNAL_WATCHER_IMPL_WINDOWS_H
#define AIPSTACK_SIGNAL_WATCHER_IMPL_WINDOWS_H

#include <cstddef>
#include <mutex>

#include <windows.h>

#include <aipstack/misc/NonCopyable.h>
#include <aipstack/misc/Use.h>
#include <aipstack/event_loop/EventLoop.h>
#include <aipstack/event_loop/SignalWatcherCommon.h>

namespace AIpStack {

class SignalWatcherImplWindows;

class SignalCollectorImplWindows :
    public SignalCollectorImplBase,
    private NonCopyable<SignalCollectorImplWindows>
{
    friend class SignalWatcherImplWindows;

    inline static constexpr std::size_t BufferSize = 32;
    
public:
    SignalCollectorImplWindows ();

    ~SignalCollectorImplWindows ();

private:
    static BOOL WINAPI consoleCtrlHandlerTrampoline (DWORD ctrlType);
    BOOL consoleCtrlHandler (DWORD ctrlType);

private:
    std::mutex m_mutex;
    SignalWatcherImplWindows *m_watcher;
    std::size_t m_buffer_start;
    std::size_t m_buffer_length;
    SignalType m_buffer[BufferSize];
};

class SignalWatcherImplWindows :
    public SignalWatcherImplBase,
    private NonCopyable<SignalWatcherImplWindows>
{
    friend class SignalCollectorImplWindows;
    
    AIPSTACK_USE_VALS(SignalCollectorImplWindows, (BufferSize))

public:
    SignalWatcherImplWindows ();

    ~SignalWatcherImplWindows ();

private:
    inline SignalCollectorImplWindows & getCollector () const;

    void asyncSignalHandler ();

private:
    EventLoopAsyncSignal m_async_signal;
};

using SignalCollectorImpl = SignalCollectorImplWindows;

using SignalWatcherImpl = SignalWatcherImplWindows;

#define AIPSTACK_SIGNAL_WATCHER_IMPL_IMPL_FILE \
    <aipstack/event_loop/platform_specific/SignalWatcherImplWindows_impl.h>

}

#endif
