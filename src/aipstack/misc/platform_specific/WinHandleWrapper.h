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

#include <cstdio>

#include <windows.h>

#include <aipstack/misc/NonCopyable.h>
#include <aipstack/misc/Assert.h>

namespace AIpStack {

/**
 * @addtogroup misc
 * @{
 */

/**
 * Wraps a Windows `HANDLE`, closing it automatically in the destructor.
 * 
 * This class is only available on Windows.
 * 
 * A @ref WinHandleWrapper object internally stores a `HANDLE` value; the object
 * is considered to have a handle when the stored value is not
 * `INVALID_HANDLE_VALUE` or null and is otherwise considered to be empty.
 * 
 * In this implementation, the value `INVALID_HANDLE_VALUE` (as opposed to null)
 * is used when explicitly making a @ref WinHandleWrapper empty. However, a null
 * value can be intorduced via the constructor @ref WinHandleWrapper(HANDLE) and
 * moved to other objects.
 */
class WinHandleWrapper :
    private AIpStack::NonCopyable<WinHandleWrapper>
{
private:
    HANDLE m_handle;
    
public:
    /**
     * Default constructor, constructs an empty object.
     * 
     * The new object gets the value `INVALID_HANDLE_VALUE`.
     */
    WinHandleWrapper () :
        m_handle(INVALID_HANDLE_VALUE)
    {}
    
    /**
     * Move constructor, takes ownership of any handle from the other object.
     * 
     * The new object gets the value of the other object and the other object
     * gets the value `INVALID_HANDLE_VALUE`.
     * 
     * @param other Object to move from.
     */
    WinHandleWrapper (WinHandleWrapper &&other) :
        m_handle(other.m_handle)
    {
        other.m_handle = INVALID_HANDLE_VALUE;
    }
    
    /**
     * Constructor from a `HANDLE` value, possibly takes ownership of a handle.
     * 
     * The new object gets the specified value.
     * 
     * @param handle Value which the new object gets. If it is `INVALID_HANDLE_VALUE`
     *        or null the result is an empty object, otherwise the value is assumed
     *        to be a handle (which would be closed in the destructor).
     */
    explicit WinHandleWrapper (HANDLE handle) :
        m_handle(handle)
    {}
    
    /**
     * Destructor, closes the handle if there is one.
     * 
     * If there is a handle in this object, it is closed by calling
     * `CloseHandle(value)`. If closing fails, an error message is printed to
     * standard error.
     */
    ~WinHandleWrapper ()
    {
        close_it();
    }
    
    /**
     * Move-assignment operator.
     * 
     * If the other object is this object then does nothing, otherwise:
     * - If there is a handle in this object, it is closed by calling
     *   `CloseHandle(value)`. If closing fails, an error message is printed to
     *   standard error.
     * - This object gets the value of the other object (possibly taking ownership
     *   of a handle) and the other object gets the value `INVALID_HANDLE_VALUE`.
     * 
     * @param other Object to move from.
     * @return \*this
     */
    WinHandleWrapper & operator= (WinHandleWrapper &&other)
    {
        if (&other != this) {
            close_it();
            m_handle = other.m_handle;
            other.m_handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }
    
    /**
     * Get the value stored in this object.
     * 
     * @return The value in the object; `INVALID_HANDLE_VALUE` or null means that
     *         there is no handle.
     */
    inline HANDLE get () const
    {
        return m_handle;
    }

    /**
     * Get the handle.
     * 
     * @note Behavior is undefined if this object does not have a handle.
     * 
     * This asserts that the object has a handle. It should be used when it is
     * logically expected that a handle is present.
     * 
     * @return The value in the object, that is the handle.
     */
    inline HANDLE operator* () const
    {
        AIPSTACK_ASSERT(handleIsValid(m_handle))
        return m_handle;
    }

    /**
     * Check whether this object has a handle.
     * 
     * @return True if the stored value is not `INVALID_HANDLE_VALUE` or null,
     *         false otherwise.
     */
    inline explicit operator bool () const
    {
        return handleIsValid(m_handle);
    }

    /**
     * Check whether a `HANDLE` value appears to be a handle.
     * 
     * @return True if `handle` is not `INVALID_HANDLE_VALUE` or null,
     *         false otherwise.
     */
    inline static bool handleIsValid (HANDLE handle)
    {
        return handle != INVALID_HANDLE_VALUE && handle != nullptr;
    }
    
private:
    void close_it ()
    {
        if (handleIsValid(m_handle)) {
            if (!::CloseHandle(m_handle)) {
                auto err = ::GetLastError();
                std::fprintf(stderr, "WinHandleWrapper: CloseHandle failed, err=%u\n",
                             (unsigned int)err);
            }
        }
        m_handle = INVALID_HANDLE_VALUE;
    }
};

/** @} */

}

#endif
