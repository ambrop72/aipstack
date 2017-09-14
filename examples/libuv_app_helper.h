/*
 * Copyright (c) 2017 Ambroz Bizjak
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

#ifndef AIPSTACK_LIBUV_APP_HELPER_H
#define AIPSTACK_LIBUV_APP_HELPER_H

#include <csignal>
#include <type_traits>

#include <uv.h>

#include <aipstack/misc/NonCopyable.h>

#include "libuv_platform.h"

namespace AIpStackExamples {

static int const WatchedSignals[] = {
SIGINT, SIGTERM, SIGHUP
#ifndef _WIN32
,SIGQUIT
#endif
};

static int const NumWatchedSignals = std::extent<decltype(WatchedSignals)>::value;

class LibuvAppHelper :
    public AIpStack::NonCopyable<LibuvAppHelper>
{
private:
    uv_loop_t m_loop;
    UvHandleWrapper<uv_signal_t> m_signals[NumWatchedSignals];
    
public:
    LibuvAppHelper ();
    
    ~LibuvAppHelper ();
    
    inline uv_loop_t * getLoop ()
    {
        return &m_loop;
    }
    
    int run ();
    
private:
    static void signalHandlerTrampoline (uv_signal_t *handle, int signum);
    void signalHandler (int signum);
};

}

#endif
