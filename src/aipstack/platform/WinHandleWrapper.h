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

#ifndef AIPSTACK_WIN_HANDLE_WRAPPER_H
#define AIPSTACK_WIN_HANDLE_WRAPPER_H

#include <windows.h>

#include <aipstack/misc/NonCopyable.h>
#include <aipstack/misc/Assert.h>

namespace AIpStack {

class WinHandleWrapper :
    private AIpStack::NonCopyable<WinHandleWrapper>
{
private:
    HANDLE m_handle;
    
public:
    WinHandleWrapper () :
        m_handle(INVALID_HANDLE_VALUE)
    {}
    
    WinHandleWrapper (WinHandleWrapper &&other) :
        m_handle(other.m_handle)
    {
        other.m_handle = INVALID_HANDLE_VALUE;
    }
    
    explicit WinHandleWrapper (HANDLE handle) :
        m_handle(handle)
    {}
    
    ~WinHandleWrapper ()
    {
        close_it();
    }
    
    WinHandleWrapper & operator= (WinHandleWrapper &&other)
    {
        if (&other != this) {
            close_it();
            m_handle = other.m_handle;
            other.m_handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }
    
    inline HANDLE get () const
    {
        return m_handle;
    }

    inline HANDLE operator* () const
    {
        AIPSTACK_ASSERT(m_handle != INVALID_HANDLE_VALUE)
        return m_handle;
    }

    inline explicit operator bool () const
    {
        return m_handle != INVALID_HANDLE_VALUE;
    }
    
private:
    void close_it ()
    {
        if (m_handle != INVALID_HANDLE_VALUE) {
            AIPSTACK_ASSERT_FORCE(CloseHandle(m_handle))
            m_handle = INVALID_HANDLE_VALUE;
        }
    }
};

}

#endif
