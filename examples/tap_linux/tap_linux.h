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

#ifndef AIPSTACK_TAP_LINUX_H
#define AIPSTACK_TAP_LINUX_H

#include <cstddef>
#include <string>
#include <vector>

#include <aipstack/misc/NonCopyable.h>
#include <aipstack/infra/Err.h>
#include <aipstack/infra/Buf.h>
#include <aipstack/platform_specific/FileDescriptorWrapper.h>
#include <aipstack/platform_impl/EventLoop.h>

namespace AIpStackExamples {

class TapDevice :
    private AIpStack::NonCopyable<TapDevice>
{
private:
    AIpStack::FileDescriptorWrapper m_fd;
    AIpStack::EventLoopFdWatcher m_fd_watcher;
    std::size_t m_frame_mtu;
    std::vector<char> m_read_buffer;
    std::vector<char> m_write_buffer;
    bool m_active;
    
public:
    TapDevice (AIpStack::EventLoop &loop, std::string const &device_id);
    ~TapDevice ();
    
    std::size_t getMtu () const;
    AIpStack::IpErr sendFrame (AIpStack::IpBufRef frame);

private:
    void handleFdEvents (AIpStack::EventLoopFdEvents events);
    
protected:
    virtual void frameReceived (AIpStack::IpBufRef frame) = 0;
};

}

#endif
