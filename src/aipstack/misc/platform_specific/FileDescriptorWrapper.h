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

#ifndef AIPSTACK_FILE_DESCRIPTOR_WRAPPER_H
#define AIPSTACK_FILE_DESCRIPTOR_WRAPPER_H

#include <cstdio>
#include <cerrno>
#include <stdexcept>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>

#include <aipstack/misc/NonCopyable.h>
#include <aipstack/misc/Assert.h>

namespace AIpStack {

/**
 * @addtogroup misc
 * @{
 */

/**
 * Wraps a file descriptor, closing it automatically in the destructor.
 * 
 * This class is only available on \*nix platforms.
 * 
 * A file descriptor object internally stores an `int` value; the object is
 * considered to have a file descriptor when the stored value is non-negative
 * and is otherwise considered to be empty.
 * 
 * An empty file descriptor object typically has the value -1, but other
 * negative values could be introduced via the @ref FileDescriptorWrapper(int)
 * constructor.
 */
class FileDescriptorWrapper :
    private AIpStack::NonCopyable<FileDescriptorWrapper>
{
private:
    int m_fd;
    
public:
    /**
     * Default constructor, constructs an empty file descriptor object.
     * 
     * The new object gets the value -1.
     */
    FileDescriptorWrapper () :
        m_fd(-1)
    {}
    
    /**
     * Move constructor, takes ownership of any file descriptor from the other
     * object.
     * 
     * The new object gets the value of the other object and the other object
     * gets the value -1.
     * 
     * @param other Object to move from.
     */
    FileDescriptorWrapper (FileDescriptorWrapper &&other) :
        m_fd(other.m_fd)
    {
        other.m_fd = -1;
    }
    
    /**
     * Constructor from an integer value, possibly takes ownership of a file
     * descriptor.
     * 
     * The new object gets the specified value.
     * 
     * @param fd Value which the new object gets. A non-negative value is assumed
     *        to be a file descriptor (which would be closed in the destructor)
     *        while a negative value results in an empty object.
     */
    explicit FileDescriptorWrapper (int fd) :
        m_fd(fd)
    {}
    
    /**
     * Destructor, closes the file descriptor if there is any.
     * 
     * This closes the file descriptor if there is one (the value is non-negative).
     * If closing fails, an error message is printed to standard error.
     */
    ~FileDescriptorWrapper ()
    {
        close_it();
    }
    
    /**
     * Move-assignment operator.
     * 
     * If the other object is this object then does nothing, otherwise:
     * - Closes the current file descriptor in this object if there is one. If
     *   closing fails, an error message is printed to standard error.
     * - This object gets the value of the other object (possibly taking ownership
     *   of a file descriptor) and the other object gets the value -1.
     * 
     * @param other Object to move from.
     * @return \*this
     */
    FileDescriptorWrapper & operator= (FileDescriptorWrapper &&other)
    {
        if (&other != this) {
            close_it();
            m_fd = other.m_fd;
            other.m_fd = -1;
        }
        return *this;
    }
    
    /**
     * Get the value stored in this object.
     * 
     * @return The value in the object; negative means that there is no file
     *         descriptor.
     */
    inline int get () const
    {
        return m_fd;
    }

    /**
     * Get the file descriptor number.
     * 
     * @note Behavior is undefined if this object does not have a file descriptor.
     * 
     * This asserts that the object has a file descriptor (the value is
     * non-negative). It should be used when it is logically expected that
     * a file descriptor is present.
     * 
     * @return The value in the object, that is the file descriptor number.
     */
    inline int operator* () const
    {
        AIPSTACK_ASSERT(m_fd >= 0)
        return m_fd;
    }

    /**
     * Check whether this object has a file descriptor.
     * 
     * @return True if the stored value is non-negative, false otherwise.
     */
    inline explicit operator bool () const
    {
        return m_fd >= 0;
    }
    
    /**
     * Set the `O_NONBLOCK` flag on the file descriptor.
     * 
     * @note Behavior is undefined if this object does not have a file descriptor.
     * 
     * This first gets the current flags using `fcntl`/`F_GETFL` and then sets new
     * flags using `fcntl`/`F_SETFL` with `O_NONBLOCK` OR-ed in.
     */
    void setNonblocking ()
    {
        AIPSTACK_ASSERT(m_fd >= 0)

        int flags = ::fcntl(m_fd, F_GETFL, 0);
        if (flags < 0) {
            throw std::runtime_error("fcntl(F_GETFL) failed.");
        }
        
        int res = ::fcntl(m_fd, F_SETFL, flags|O_NONBLOCK);
        if (res == -1) {
            throw std::runtime_error("fcntl(F_SETFL, flags|O_NONBLOCK) failed.");
        }
    }

    /**
     * Utility function to check if an error code is `EAGAIN` or `EWOULDBLOCK`.
     * 
     * This seems trivial but it solves the problem that an expression such as
     * `err == EAGAIN || err == EWOULDBLOCK` can result in a compiler warning
     * when the values `EAGAIN` of `EWOULDBLOCK` are the same, as they are on
     * Linux.
     * 
     * @param err Error code value to test.
     * @return True if the value is `EAGAIN` or `EWOULDBLOCK`, false otherwise.
     */
    static bool errIsEAGAINorEWOULDBLOCK (int err)
    {
        if (err == EAGAIN) {
            return true;
        }
        #if EWOULDBLOCK != EAGAIN
        if (err == EWOULDBLOCK) {
            return true;
        }
        #endif
        return false;
    }
    
private:
    void close_it ()
    {
        if (m_fd >= 0) {
            if (::close(m_fd) < 0) {
                int err = errno;
                std::fprintf(stderr, "FileDescriptorWrapper: close failed, errno=%d\n",
                             err);
            }
            m_fd = -1;
        }
    }
};

/** @} */

}

#endif
