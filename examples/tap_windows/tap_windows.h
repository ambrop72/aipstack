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

#ifndef AIPSTACK_TAP_WINDOWS_H
#define AIPSTACK_TAP_WINDOWS_H

#include <cstddef>
#include <string>
#include <memory>
#include <vector>

#include <uv.h>

#include <aipstack/misc/NonCopyable.h>
#include <aipstack/common/Err.h>
#include <aipstack/common/Buf.h>

#include "../libuv_platform.h"
#include "win_handle_wrapper.h"

namespace AIpStackExamples {

class TapDevice :
    private AIpStack::NonCopyable<TapDevice>
{
    static std::size_t const NumSendBuffers = 64;
    
    struct OverlappedUserData {
        TapDevice *parent;
        std::shared_ptr<WinHandleWrapper> device;
        std::vector<char> buffer;
        bool active;
    };
    
    using OverlappedWrapper = UvHandleWrapper<uv_overlapped_t, OverlappedUserData>;
    
private:
    uv_loop_t *m_loop;
    std::shared_ptr<WinHandleWrapper> m_device;
    std::size_t m_frame_mtu;
    std::size_t m_send_first;
    std::size_t m_send_count;
    OverlappedWrapper m_uv_olap_send[NumSendBuffers];
    OverlappedWrapper m_uv_olap_recv;
    
public:
    TapDevice (uv_loop_t *loop, std::string const &device_id);
    ~TapDevice ();
    
    std::size_t getMtu () const;
    AIpStack::IpErr sendFrame (AIpStack::IpBufRef frame);
    
protected:
    virtual void frameReceived (AIpStack::IpBufRef frame) = 0;
    
private:
    static std::size_t send_ring_add (std::size_t a, std::size_t b);
    static std::size_t send_ring_sub (std::size_t a, std::size_t b);
    
    void initOverlapped (OverlappedWrapper &wrapper);
    
    bool startRecv ();
    
    template <void (TapDevice::*Cb) (OverlappedWrapper &)>
    static void olapCbTrampoline (uv_overlapped_t *handle);
    
    void olapSendCb (OverlappedWrapper &wrapper);
    void olapRecvCb (OverlappedWrapper &wrapper);
};

}

#endif
