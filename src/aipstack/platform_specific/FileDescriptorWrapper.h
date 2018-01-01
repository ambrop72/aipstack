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

namespace AIpStack {

class FileDescriptorWrapper :
    private AIpStack::NonCopyable<FileDescriptorWrapper>
{
private:
    int m_fd;
    
public:
    FileDescriptorWrapper () :
        m_fd(-1)
    {}
    
    FileDescriptorWrapper (FileDescriptorWrapper &&other) :
        m_fd(other.m_fd)
    {
        other.m_fd = -1;
    }
    
    explicit FileDescriptorWrapper (int fd) :
        m_fd(fd)
    {}
    
    ~FileDescriptorWrapper ()
    {
        close_it();
    }
    
    FileDescriptorWrapper & operator= (FileDescriptorWrapper &&other)
    {
        if (&other != this) {
            close_it();
            m_fd = other.m_fd;
            other.m_fd = -1;
        }
        return *this;
    }
    
    inline int get () const
    {
        return m_fd;
    }
    
    void setNonblocking ()
    {
        int flags = ::fcntl(m_fd, F_GETFL, 0);
        if (flags < 0) {
            throw std::runtime_error("fcntl(F_GETFL) failed.");
        }
        
        int res = ::fcntl(m_fd, F_SETFL, flags|O_NONBLOCK);
        if (res == -1) {
            throw std::runtime_error("fcntl(F_SETFL, flags|O_NONBLOCK) failed.");
        }
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

}

#endif
