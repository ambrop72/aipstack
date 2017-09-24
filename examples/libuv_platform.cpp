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

#include <cassert>

#include "libuv_platform.h"

namespace AIpStackExamples {

PlatformImplLibuv::PlatformImplLibuv (uv_loop_t *loop) :
    m_loop(loop)
{
}

PlatformImplLibuv::Timer::Timer (ThePlatformRef ref) :
    ThePlatformRef(ref),
    m_is_set(false)
{
    int res = m_handle.initialize([&](uv_timer_t *dst) {
        return uv_timer_init(platformImpl()->loop(), dst);
    });
    assert(res == 0); (void)res;
    
    m_handle.get()->data = this;
}

PlatformImplLibuv::Timer::~Timer ()
{
}

void PlatformImplLibuv::Timer::uvTimerHandlerTrampoline (uv_timer_t *timer)
{
    Timer *obj = reinterpret_cast<Timer *>(timer->data);
    assert(timer == obj->m_handle.get());
    
    obj->uvTimerHandler();
}

void PlatformImplLibuv::Timer::uvTimerHandler ()
{
    assert(m_is_set);
    
    m_is_set = false;
    
    handleTimerExpired();
}

}
