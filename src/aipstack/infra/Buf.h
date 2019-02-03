/*
 * Copyright (c) 2016 Ambroz Bizjak
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

#ifndef AIPSTACK_BUF_H
#define AIPSTACK_BUF_H

#include <cstddef>

#include <aipstack/misc/Assert.h>
#include <aipstack/misc/MinMax.h>

namespace AIpStack {
/**
 * @ingroup infra
 * @defgroup buffer Buffer Chain Infrastructure
 * @brief Provides infrastructure for working with possibly discontiguous byte sequences.
 * 
 * The buffer chain infrastructure is used throughout the TCP/IP stack implementation and
 * enables simple and efficient passing of data between different layers of the stack.
 * 
 * The @ref IpBufNode structure represents a node within a singly-linked chain of buffers
 * and the @ref IpBufRef structure references a byte sequence within such a chain. These
 * facilities are essentially tools for working with possibly discontiguous byte sequences
 * and nothing more. They do not perform any memory management or know anything about
 * network protocols or protocol layers.
 * 
 * Applications typically need to work with these facilities at two opposite levels:
 * - In network interface drivers. In this case, the driver uses @ref IpBufRef
 *   to describe the contents of a packet or frame being passed into the stack for
 *   processing, and the stack likewise uses @ref IpBufRef to describe the contents of
 *   a packet or frame which is to be sent through the network interface.
 * - In application code utilizing transport-layer protocols. For TCP, the application
 *   allocates the send and receive buffers while the TCP implementation manages two
 *   @ref IpBufRef objects: one which references outgoing data in the send buffer (unsent
 *   and unacknowledged), and one which references the available memory space for storing
 *   received data. See the @ref TcpConnection class for more information.
 * 
 * @{
 */

/**
 * Node in a chain of buffers.
 * 
 * A buffer node is defined by a pointer to the start of a buffer (@ref ptr), the length
 * of the buffer (@ref len) and a pointer to the next buffer node (@ref next), if any.
 */
struct IpBufNode {
    /**
     * Pointer to the buffer data.
     */
    char *ptr = nullptr;
    
    /**
     * Length of the buffer.
     */
    std::size_t len = 0;
    
    /**
     * Pointer to the next buffer node, or null if this is the end of the chain.
     */
    IpBufNode const *next = nullptr;
};

/**
 * Reference to a possibly discontiguous byte sequence within a chain of buffers.
 * 
 * It contains a pointer to the first buffer node (@ref node), the byte offset within
 * the first buffer (@ref offset), and the total length of the referenced byte sequence
 * (@ref tot_len).
 * 
 * A valid @ref IpBufRef object defines a logical byte sequence. This byte sequence
 * is obtained if one starts at the position @ref offset within the first buffer (defined
 * by @ref node) then traverses the bytes in the buffer chain until exactly @ref tot_len
 * bytes have been found. That is, when one reaches the end of one buffer and and less than
 * @ref tot_len bytes have been found, one continues at the start of the next buffer as
 * pointed to by @ref IpBufNode::next.
 * 
 * Except where noted otherwise, all functions in @ref IpBufRef require the reference to
 * be *valid*. Specifically, the following must hold for a reference to be valid:
 * 1. @ref node is not null and points to an existing @ref IpBufNode.
 * 2. @ref offset is less than or equal to `node->len`.
 * 3. There are at least @ref tot_len bytes available in the buffer chain when starting at
 *    @ref offset in the first buffer (defined by @ref node). This implies that for each
 *    buffer where the byte sequence does not yet end, @ref IpBufNode::next is not null and
 *    points to an existing @ref IpBufNode where the byte sequence continues.
 * 4. If the end of the referenced byte sequence coincides with the end of a buffer, it
 *    must be safe to continue traversing the chain of buffers until a buffer with
 *    non-zero length is encountered (@ref IpBufNode::len is nonzero) or the end of the
 *    chain is reached (@ref IpBufNode::next is null). This is required so that
 *    it is safe for @ref ipBufProcessBytes and other processing functions to move to
 *    subsequent buffers eagerly.
 * 
 * Functions in @ref IpBufRef never never modify @ref IpBufNode objects; rather,
 * @ref IpBufRef objects may be modified or new ones created, often to refer to a different
 * but related byte sequence.
 */
struct IpBufRef {
    /**
     * Pointer to the first buffer node.
     */
    IpBufNode const *node = nullptr;
    
    /**
     * Byte offset in the first buffer.
     */
    std::size_t offset = 0;
    
    /**
     * The total length of the data range.
     */
    std::size_t tot_len = 0;
    
    /**
     * Return the pointer to the first chunk of the memory range.
     * 
     * @return `node->ptr + offset`
     */
    inline char * getChunkPtr () const
    {
        assertBufSanity(*this);
        
        return node->ptr + offset;
    }
    
    /**
     * Return the length of the first chunk of the memory range.
     * 
     * @return `min(tot_len, node->len - offset)`
     */
    inline std::size_t getChunkLength () const
    {
        assertBufSanity(*this);
        
        return MinValue(tot_len, std::size_t(node->len - offset));
    }
    
    /**
     * Extend the memory range backward in the first buffer.
     * 
     * @note There must in fact be `amount` bytes available to reveal, that is
     * @ref offset >= `amount`.
     * 
     * @param amount Number od bytes to reveal. Must be less then or equal
     *        to @ref offset.
     * @return The adjusted memory range.
     */
    inline IpBufRef revealHeader (std::size_t amount) const
    {
        AIPSTACK_ASSERT(amount <= offset);
        
        return IpBufRef {
            node,
            std::size_t(offset  - amount),
            std::size_t(tot_len + amount)
        };
    }
    
    /**
     * Check if there are at least `amount` bytes available in the first chunk of
     * this memory range.
     * 
     * @param amount Number of bytes to check for.
     * @return `amount <= getChunkLength()`.
     */
    inline bool hasHeader (std::size_t amount) const
    {
        assertBufSanity(*this);
        
        return amount <= tot_len && amount <= node->len - offset;
    }
    
    /**
     * Return a memory range without an initial portion of this memory range
     * that must be within the first chunk.
     * 
     * @param amount Number of bytes to hide. Must be less than or equal
     *        to @ref getChunkLength().
     * @return A reference with the same @ref node, @ref offset incremented
     *         by `amount` and @ref tot_len decremented by `amount`.
     */
    inline IpBufRef hideHeader (std::size_t amount) const
    {
        assertBufSanity(*this);
        AIPSTACK_ASSERT(amount <= tot_len);
        AIPSTACK_ASSERT(amount <= node->len - offset);
        
        return IpBufRef {
            node,
            std::size_t(offset  + amount),
            std::size_t(tot_len - amount)
        };
    }
    
    /**
     * Return a memory range that is a prefix of this memory range.
     * 
     * It returns an @ref IpBufRef with the same @ref node and @ref offset
     * but with @ref tot_len equal to `new_tot_len`.
     * 
     * @ref node is allowed to be null.
     * 
     * @param new_tot_len The length of the prefix memory range. Must be less than
     *        or equal to @ref tot_len.
     * @return The prefix range whose length is `new_tot_len`.
     */
    inline IpBufRef subTo (std::size_t new_tot_len) const
    {
        AIPSTACK_ASSERT(new_tot_len <= tot_len);
        
        return IpBufRef {
            node,
            offset,
            new_tot_len
        };
    }
    
    /**
     * Assert that a buffer is sane.
     * 
     * This asserts that @ref node is not null and that @ref offset does not exceed
     * @ref IpBufNode::len of the node.
     * 
     * @param buf Buffer to check.
     */
    static void assertBufSanity (IpBufRef buf)
    {
        AIPSTACK_ASSERT(buf.node != nullptr);
        AIPSTACK_ASSERT(buf.offset <= buf.node->len);
    }
};

/** @} */
}

#endif
