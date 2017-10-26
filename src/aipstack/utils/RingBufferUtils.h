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

#ifndef AIPSTACK_RING_BUFFER_UTILS_H
#define AIPSTACK_RING_BUFFER_UTILS_H

#include <stddef.h>

#include <aipstack/misc/Assert.h>
#include <aipstack/misc/Modulo.h>
#include <aipstack/infra/Buf.h>

namespace AIpStack {

/**
 * Representation of a range in a circular buffer.
 * 
 * This is used as the return type of @ref calcRingBufComplement.
 */
struct RingBufRange {
    /**
     * Starting position in the buffer.
     */
    size_t pos;
    
    /**
     * Length of the range.
     */
    size_t len;
};

/**
 * Calculate the complement of a ring buffer range described by an @ref IpBufRef.
 * 
 * This is intended to be used with TCP receive and send buffers when the application
 * has configured the buffer as a simple circular buffer, that is with a single
 * @ref IpBufNode whose 'next' pointer points back to the same node.
 * 
 * For example:
 * - When used with @ref TcpConnection::getRecvBuf (which describes the
 *   available space for newly received data), this function computes the range of
 *   received unprocessed data.
 * - When used with @ref TcpConnection::getSendBuf (which describes the
 *   data submitted for sending but not yet acknowledged), this function computes the
 *   range of free space where new data to be sent should be written.
 * 
 * Formally, the following is required by this function:
 * - 'ref.node' must point to an @ref IpBufNode whose 'next' member points back to the
 *   same node.
 * - The modulus `mod.modulus()` must be equal to the 'len' of the node, that is the
 *   size of the circular buffer. This is passed by argument for efficiency reasons.
 * - 'ref.offset' must be less than the modulus (never equal to). This should be
 *   satsified if used with @ref TcpConnection since it provides the guarantee of "eager
 *   advancement to subsequent buffer nodes".
 * - 'ref.tot_len' must be less than or equal to the buffer size. This is also
 *   automatically satisfied when the circular buffer is used correctly.
 * 
 * The results of this function are:
 * - @ref RingBufRange::pos is the starting position of the complement range (always
 *   less than 'mod'). It is equal to `mod.add(ref.offset, ref.tot_len)`.
 * - @ref RingBufRange::len is the length of the complement. It is equal to
 *   `mod.modulusComplement(ref.tot_len)`.
 * 
 * @param ref Reference to a range within a circular buffer (see requirements).
 * @param mod Size of the circular buffer given as a @ref Modulo modulus value.
 * @return A description of the complement range.
 */
inline RingBufRange calcRingBufComplement (IpBufRef ref, Modulo mod)
{
    AIPSTACK_ASSERT(ref.node != nullptr)
    AIPSTACK_ASSERT(ref.node->len == mod.modulus())
    AIPSTACK_ASSERT(ref.node->next == ref.node)
    AIPSTACK_ASSERT(ref.offset < mod.modulus())
    AIPSTACK_ASSERT(ref.tot_len <= mod.modulus())
    
    RingBufRange result;
    result.pos = mod.add(ref.offset, ref.tot_len);
    result.len = mod.modulusComplement(ref.tot_len);
    return result;
}

}

#endif
