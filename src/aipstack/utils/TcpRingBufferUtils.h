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

#ifndef AIPSTACK_TCP_RING_BUFFER_UTILS_H
#define AIPSTACK_TCP_RING_BUFFER_UTILS_H

#include <stddef.h>
#include <string.h>

#include <aipstack/misc/Assert.h>
#include <aipstack/misc/MinMax.h>
#include <aipstack/misc/Modulo.h>
#include <aipstack/misc/Hints.h>
#include <aipstack/infra/Buf.h>
#include <aipstack/infra/MemRef.h>

namespace AIpStack {

template <typename TcpProto>
class SendRingBuffer {
public:
    using Connection = typename TcpProto::Connection;

    void setup (Connection &con, char *buf, size_t buf_size)
    {
        AIPSTACK_ASSERT(buf != nullptr)
        AIPSTACK_ASSERT(buf_size > 0)
        AIPSTACK_ASSERT(buf_size >= con.getSendBuf().tot_len)
        
        m_buf_node = IpBufNode{buf, buf_size, &m_buf_node};
        
        IpBufRef old_send_buf = con.getSendBuf();

        IpBufRef send_buf = IpBufRef{&m_buf_node, (size_t)0, old_send_buf.tot_len};

        if (old_send_buf.tot_len > 0) {
            IpBufRef tmp_buf = send_buf;
            tmp_buf.giveBuf(old_send_buf);
        }

        con.setSendBuf(send_buf);
    }
    
    inline size_t getFreeLen (Connection &con) const
    {
        IpBufRef send_buf = getSendBuf(con);
        return getModulo().modulusComplement(send_buf.tot_len);
    }
    
    IpBufRef getWriteRange (Connection &con) const
    {
        IpBufRef send_buf = getSendBuf(con);
        size_t write_offset = getModulo().add(send_buf.offset, send_buf.tot_len);
        size_t free_len = getModulo().modulusComplement(send_buf.tot_len);
        return IpBufRef{&m_buf_node, write_offset, free_len};
    }
    
    inline void provideData (Connection &con, size_t amount)
    {
        AIPSTACK_ASSERT(amount <= getFreeLen(con))
        
        con.extendSendBuf(amount);
    }
    
    void writeData (Connection &con, MemRef data)
    {
        AIPSTACK_ASSERT(data.len <= getFreeLen(con))
        
        IpBufRef write_range = getWriteRange(con);
        write_range.giveBytes(data.len, data.ptr);

        con.extendSendBuf(data.len);
    }
    
private:
    inline Modulo getModulo () const
    {
        return Modulo(m_buf_node.len);
    }

    inline IpBufRef getSendBuf (Connection &con) const
    {
        IpBufRef send_buf = con.getSendBuf();

        // Assert expectations. The second is due to eager advancement to subsequent
        // buffer nodes guaranteed by TcpConnection.
        AIPSTACK_ASSERT(send_buf.tot_len <= getModulo().modulus())
        AIPSTACK_ASSERT(send_buf.offset < getModulo().modulus())

        return send_buf;
    }
    
private:
    IpBufNode m_buf_node;
};

template <typename TcpProto>
class RecvRingBuffer {
public:
    using Connection = typename TcpProto::Connection;

    // NOTE: If using mirror region and initial_rx_data is not empty, it is
    // you may need to call updateMirrorAfterDataReceived to make sure initial
    // data is mirrored as applicable.
    void setup (Connection &con, char *buf, size_t buf_size, int wnd_upd_div,
                IpBufRef initial_rx_data = IpBufRef{})
    {
        AIPSTACK_ASSERT(buf != nullptr)
        AIPSTACK_ASSERT(buf_size > 0)
        AIPSTACK_ASSERT(wnd_upd_div >= 2)
        AIPSTACK_ASSERT(initial_rx_data.tot_len <= buf_size)
        AIPSTACK_ASSERT(buf_size - initial_rx_data.tot_len >= con.getRecvBuf().tot_len)
        
        m_buf_node = IpBufNode{buf, buf_size, &m_buf_node};
        
        con.setProportionalWindowUpdateThreshold(buf_size, wnd_upd_div);
        
        IpBufRef old_recv_buf = con.getRecvBuf();

        IpBufRef recv_buf = IpBufRef{&m_buf_node, (size_t)0, buf_size};
        
        if (initial_rx_data.tot_len > 0) {
            recv_buf.giveBuf(initial_rx_data);
        }
        
        if (old_recv_buf.tot_len > 0) {
            IpBufRef tmp_buf = recv_buf;
            tmp_buf.giveBuf(old_recv_buf);
        }
        
        con.setRecvBuf(recv_buf);
    }
    
    inline size_t getUsedLen (Connection &con) const
    {
        IpBufRef recv_buf = getRecvBuf(con);
        return getModulo().modulusComplement(recv_buf.tot_len);
    }
    
    IpBufRef getReadRange (Connection &con)
    {
        IpBufRef recv_buf = getRecvBuf(con);
        size_t read_offset = getModulo().add(recv_buf.offset, recv_buf.tot_len);
        size_t used_len = getModulo().modulusComplement(recv_buf.tot_len);
        return IpBufRef{&m_buf_node, read_offset, used_len};
    }
    
    inline void consumeData (Connection &con, size_t amount)
    {
        AIPSTACK_ASSERT(amount <= getUsedLen(con))
        
        con.extendRecvBuf(amount);
    }
    
    void readData (Connection &con, MemRef data)
    {
        AIPSTACK_ASSERT(data.len <= getUsedLen(con))
        
        IpBufRef read_range = getReadRange(con);
        read_range.takeBytes(data.len, (char *)data.ptr);

        con.extendRecvBuf(data.len);
    }
    
    void updateMirrorAfterReceived (Connection &con, size_t mirror_size, size_t amount)
    {
        AIPSTACK_ASSERT(mirror_size >= 0)
        AIPSTACK_ASSERT(mirror_size <= getModulo().modulus())
        
        if (AIPSTACK_UNLIKELY(amount == 0)) {
            return;
        }
    
        Modulo mod = getModulo();

        IpBufRef recv_buf = con.getRecvBuf();
        AIPSTACK_ASSERT(recv_buf.tot_len + amount <= mod.modulus())
        AIPSTACK_ASSERT(recv_buf.offset < mod.modulus())

        // Calculate the offset in the buffer to which new data was written and
        // the number of bytes wrap-around.
        size_t data_offset = mod.sub(recv_buf.offset, amount);
        size_t wrap_len = mod.modulusComplement(data_offset);
        
        // Copy initial data to the mirror region as needed.
        if (data_offset < mirror_size) {
            ::memcpy(
                m_buf_node.ptr + mod.modulus() + data_offset,
                m_buf_node.ptr + data_offset,
                MinValue(amount, size_t(mirror_size - data_offset)));
        }

        // Copy final data to the mirror region as needed.
        if (amount > wrap_len) {
            ::memcpy(
                m_buf_node.ptr + mod.modulus(),
                m_buf_node.ptr,
                MinValue(size_t(amount - wrap_len), mirror_size));
        }
    }
    
private:
    inline Modulo getModulo () const
    {
        return Modulo(m_buf_node.len);
    }
    
    inline IpBufRef getRecvBuf (Connection &con) const
    {
        IpBufRef recv_buf = con.getRecvBuf();

        // Assert expectations. The second is due to eager advancement to subsequent
        // buffer nodes guaranteed by TcpConnection.
        AIPSTACK_ASSERT(recv_buf.tot_len <= getModulo().modulus())
        AIPSTACK_ASSERT(recv_buf.offset < getModulo().modulus())

        return recv_buf;
    }
    
private:
    IpBufNode m_buf_node;
};

}

#endif
