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

#ifndef AIPSTACK_LIBUV_PLATFORM_H
#define AIPSTACK_LIBUV_PLATFORM_H

#include <cassert>
#include <cstdint>
#include <limits>

#include <uv.h>

#include <aipstack/misc/NonCopyable.h>
#include <aipstack/platform/PlatformFacade.h>

namespace AIpStackExamples {

class UvHandleWrapperEmptyUserData {};

template <typename HandleType, typename UserData = UvHandleWrapperEmptyUserData>
class UvHandleWrapper :
    private AIpStack::NonCopyable<UvHandleWrapper<HandleType>>
{
    struct DynAllocPart;
    
private:
    DynAllocPart *m_dyn;
    
public:
    UvHandleWrapper () :
        m_dyn(nullptr)
    {}
    
    ~UvHandleWrapper ()
    {
        detach();
    }
    
    template <typename Func>
    int initialize (Func func)
    {
        assert(m_dyn == nullptr);
        
        m_dyn = new DynAllocPart;
        
        int res = func(&m_dyn->handle);
        
        if (res != 0) {
            delete m_dyn;
            m_dyn = nullptr;
        }
        
        return res;
    }
    
    void detach ()
    {
        if (m_dyn != nullptr) {
            m_dyn->handle.data = m_dyn;
            uv_close((uv_handle_t *)&m_dyn->handle, &UvHandleWrapper::closeHandler);
            m_dyn = nullptr;
        }
    }
    
    HandleType * get ()
    {
        assert(m_dyn != nullptr);
        
        return &m_dyn->handle;
    }
    
    UserData & user ()
    {
        assert(m_dyn != nullptr);
        
        return m_dyn->user_data;
    }
    
private:
    struct DynAllocPart {
        HandleType handle;
        UserData user_data;
    };
    
    static void closeHandler (uv_handle_t *handle_arg)
    {
        HandleType *handle = reinterpret_cast<HandleType *>(handle_arg);
        DynAllocPart *dyn = reinterpret_cast<DynAllocPart *>(handle->data);
        assert(handle == &dyn->handle);
        
        delete dyn;
    }
};

class PlatformImplLibuv :
    private AIpStack::NonCopyable<PlatformImplLibuv>
{
private:
    uv_loop_t *m_loop;
    
public:
    using ThePlatformRef = AIpStack::PlatformRef<PlatformImplLibuv>;
    
    static bool const ImplIsStatic = false;
    
    using TimeType = std::uint64_t;
    
    static constexpr double TimeFreq = 1000.0;
    
    static constexpr TimeType RelativeTimeLimit =
        std::numeric_limits<TimeType>::max() / 64;
    
public:
    PlatformImplLibuv (uv_loop_t *loop);
    
    inline uv_loop_t * loop () const
    {
        return m_loop;
    }
    
    inline TimeType getTime ()
    {
        return uv_now(m_loop);
    }
    
    inline TimeType getEventTime ()
    {
        return uv_now(m_loop);
    }
    
    class Timer :
        private ThePlatformRef,
        private AIpStack::NonCopyable<Timer>
    {
        using ThePlatformRef::platformImpl;
        
        struct TimerDynAlloc;
        
    private:
        bool m_is_set;
        TimeType m_set_time;
        UvHandleWrapper<uv_timer_t> m_handle;
        
    public:
        Timer (ThePlatformRef ref);
        
        ~Timer ();
        
        using ThePlatformRef::ref;
        
        inline bool isSet () const
        {
            return m_is_set;
        }
        
        inline TimeType getSetTime () const
        {
            return m_set_time;
        }
        
        void unset ()
        {
            if (m_is_set) {
                m_is_set = false;
                
                int res = uv_timer_stop(m_handle.get());
                assert(res == 0); (void)res;
            }
        }
        
        void setAt (TimeType abs_time)
        {
            m_is_set = true;
            m_set_time = abs_time;
            
            TimeType now = platformImpl()->getEventTime();
            TimeType timeout = (abs_time >= now) ? (abs_time - now) : 0;
            
            int res = uv_timer_start(m_handle.get(), &Timer::uvTimerHandlerTrampoline,
                                     timeout, 0);
            assert(res == 0); (void)res;
        }
        
    protected:
        virtual void handleTimerExpired () = 0;
        
    private:
        static void uvTimerHandlerTrampoline (uv_timer_t *timer);
        void uvTimerHandler ();
    };
};

}

#endif
