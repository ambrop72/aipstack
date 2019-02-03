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
#include <cstring>

#include <aipstack/misc/Assert.h>
#include <aipstack/misc/Hints.h>
#include <aipstack/misc/MinMax.h>
#include <aipstack/misc/MemRef.h>

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
 *    it is safe for @ref processBytes and other processing functions to move to subsequent
 *    buffers eagerly.
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
     * Return an @ref IpBufNode corresponding to the first buffer
     * of the memory range with the offset applied.
     * 
     * @return An @ref IpBufNode with `ptr` equal to `node->ptr + offset`,
     *        `len` equal to `node->len - offset` and `next` equal to
     *        `node->next`.
     */
    inline IpBufNode toNode () const
    {
        assertBufSanity(*this);
        
        return IpBufNode {
            node->ptr + offset,
            std::size_t(node->len - offset),
            node->next
        };
    }
    
    /**
     * Construct a memory range consisting of an initial portion
     * of the first chunk of this memory range continued by
     * data in a specified buffer chain.
     * 
     * It sets `*out_node` to a new @ref IpBufNode referencing the
     * initial portion and continuing into the given buffer
     * chain (`ptr = node->ptr`, `len = offset + header_len`,
     * `next = cont`), and returns an @ref IpBufRef using `out_node`
     * as its first buffer (`node = out_node`, `offset = offset`,
     * `tot_len = total_len`).
     * 
     * Note that this does not "apply" the offset to the node
     * as @ref toNode does. This is to allow @ref revealHeader.
     *
     * It is important to understand that this works by creating
     * a new @ref IpBufNode, because the buffer chain model cannot
     * support this operation otherwise. The returned @ref IpBufRef
     * will be valid only so long as `out_node` remains valid.
     * 
     * @param header_len Length of the initial portion. Must be less than or
     *        equal to `node->len - offset`.
     * @param cont Pointer to the buffer node with data after the initial
     *        portion (may be null if there is no such data).
     * @param total_len Total length of the constructed memory range.
     *        Must be greater than or equal to `header_len`.
     * @param out_node Pointer to where the new @ref IpBufNode for the
     *        initial portion will be written (must not be null).
     * @return An @ref IpBufRef referencing the constructed memory range.
     */
    IpBufRef subHeaderToContinuedBy (std::size_t header_len, IpBufNode const *cont,
                                     std::size_t total_len, IpBufNode *out_node) const
    {
        assertBufSanity(*this);
        AIPSTACK_ASSERT(header_len <= node->len - offset);
        AIPSTACK_ASSERT(total_len >= header_len);
        
        *out_node = IpBufNode{node->ptr, std::size_t(offset + header_len), cont};
        return IpBufRef{out_node, offset, total_len};
    }
    
    /**
     * Consume a number of bytes from the front of the memory range.
     * 
     * This moves to subsequent buffers eagerly (see @ref processBytes).
     * 
     * @param amount Number of bytes to consume. Must be less than or equal
     *        to @ref tot_len.
     */
    void skipBytes (std::size_t amount)
    {
        processBytes(amount, [](char *, std::size_t) {});
    }
    
    /**
     * Consume a number of bytes from the front of the memory
     * memory range while copying them to the given memory
     * location.
     * 
     * This moves to subsequent buffers eagerly (see @ref processBytes).
     * 
     * @param amount Number of bytes to copy out and consume. Must be less than
     *        or equal to @ref tot_len.
     * @param dst Location to copy to. May be null only if `amount` is zero.
     */
    void takeBytes (std::size_t amount, char *dst)
    {
        processBytes(amount, [&](char *data, std::size_t len) {
            std::memcpy(dst, data, len);
            dst += len;
        });
    }
    
    /**
     * Consume a number of bytes from the front of the memory range while copying bytes
     * from the given memory location into the consumed part of the range.
     * 
     * This moves to subsequent buffers eagerly (see @ref processBytes).
     * 
     * @param data Reference to bytes to copy in (as @ref MemRef). `data.len` must be less
     *        than or equal to @ref tot_len. `data.ptr` may be null if `data.len` is zero.
     */
    void giveBytes (MemRef data)
    {
        char const *src = data.ptr;
        processBytes(data.len, [&](char *cdata, std::size_t clen) {
            std::memcpy(cdata, src, clen);
            src += clen;
        });
    }
    
    /**
     * Consume a number of bytes from the front of the memory
     * range while copying bytes from another memory range
     * into the consumed part of the range.
     * 
     * The number of bytes consumed and copied is equal to
     * the length of the other memory range (`src`), and must
     * not exceed the length of this memory range.
     * 
     * This moves to subsequent buffers eagerly (see @ref processBytes).
     * 
     * @param src Memory range to copy in. `src.tot_len` must be less than
     *        or equal to @ref tot_len of this memory range.
     */
    void giveBuf (IpBufRef src)
    {
        processBytes(src.tot_len, [&](char *data, std::size_t len) {
            src.takeBytes(len, data);
        });
    }
    
    /**
     * Consume and return a single byte from the front of the
     * memory range.
     * 
     * @ref tot_len must be positive.
     * 
     * This moves to subsequent buffers eagerly (see @ref processBytes).
     * 
     * @return The consumed byte.
     */
    char takeByte ()
    {
        AIPSTACK_ASSERT(tot_len > 0);
        
        char ch = 0;
        processBytes(1, [&](char *data, [[maybe_unused]] std::size_t len) {
            ch = *data;
        });
        return ch;
    }
    
    /**
     * Set a number of bytes at the front of the memory range to a specific value and
     * consume them.
     * 
     * This moves to subsequent buffers eagerly (see @ref processBytes).
     * 
     * @param byte Value to set bytes to.
     * @param amount Number of bytes to set and consume. Must be less than or equal to
     *        @ref tot_len.
     */
    void giveSameBytes (char byte, std::size_t amount)
    {
        processBytes(amount, [&](char *data, std::size_t len) {
            std::memset(data, byte, len);
        });
    }

    /**
     * Search for the first occurrence of the specified byte while consuming bytes from
     * the front of the memory range.
     *
     * If `byte` is found within the first `amount` bytes, this function returns true and
     * all bytes up to and including the first occurrence have been consumed. Otherwise,
     * this function returns false and min(`amount`, original @ref tot_len) bytes have been
     * consumed.
     * 
     * This moves to subsequent buffers eagerly (see @ref processBytesInterruptible).
     * 
     * @param byte Byte to search for.
     * @param amount Maximum number of bytes to process and consume. In any case no more
     *        than @ref tot_len bytes will be processed.
     */
    bool findByte (char byte, std::size_t amount = std::size_t(-1))
    {
        return processBytesInterruptible(amount, [&](char *data, std::size_t &len) {
            void *ch = std::memchr(data, byte, len);
            if (ch == nullptr) {
                return false;
            } else {
                len = std::size_t((reinterpret_cast<char *>(ch) - data) + 1);
                return true;
            }
        });
    }

    /**
     * Check if this memory range begins with a specific prefix and if so return the
     * reference to the remainder of the range.
     * 
     * This function does not modify this object but returns the remainder via the
     * `remainder` output parameter, if the prefix is found.
     * 
     * This moves to subsequent buffers eagerly (see @ref processBytesInterruptible), with
     * respect to the returned `remainder`.
     * 
     * @param prefix Prefix to check for (`prefix.ptr` may be null if `prefix.len` is 0).
     * @param remainder If the prefix is found, is set to a reference to the remainder of
     *        this memory range following the prefix (not modified if the prefix is not
     *        found).
     * @return True if the prefix is found, false if not.
     */
    bool startsWith (MemRef prefix, IpBufRef &remainder) const
    {
        if (prefix.len > tot_len) {
            return false;
        }

        IpBufRef copy_ref = *this;
        std::size_t position = 0;

        bool mismatch = copy_ref.processBytesInterruptible(prefix.len,
        [&](char *data, std::size_t &len) {
            if (std::memcmp(data, prefix.ptr + position, len) != 0) {
                return true;
            }
            position += len;
            return false;
        });

        if (mismatch) {
            return false;
        }

        AIPSTACK_ASSERT(copy_ref.tot_len == tot_len - prefix.len);

        remainder = copy_ref;
        return true;
    }
    
    /**
     * Process and consume a number of bytes from the front of the memory range by
     * invoking a callback function on contiguous chunks.
     * 
     * The function will be called on the subsequent contiguous
     * chunks of the consumed part of this memory range. It will
     * be passed two arguments: a char pointer to the start of
     * the chunk, and a size_t length of the chunk. The function
     * will not be called on zero-sized chunks.
     * 
     * This function moves forward to subsequent buffers eagerly. This means that when
     * there are no more bytes to be processed, it will move to the next buffer as long as
     * it is at the end of the current buffer and there is a next buffer.
     * 
     * This eager moving across buffer is useful when the buffer
     * chain is a ring buffer, so that the offset into the buffer
     * will remain always less than the buffer size, never becoming
     * equal.
     * 
     * @tparam Func Function object type.
     * @param amount Number of bytes to process and consume. Must be less than
     *        or equal to @ref tot_len.
     * @param func Function to call in order to process chunks (see above). The
     *        function must not modify this @ref IpBufRef object, and the state of
     *        this object at the time of invocation is unspecified.
     */
    template<typename Func>
    void processBytes (std::size_t amount, Func func)
    {
        AIPSTACK_ASSERT(node != nullptr);
        AIPSTACK_ASSERT(amount <= tot_len);
        
        while (true) {
            AIPSTACK_ASSERT(offset <= node->len);
            std::size_t rem_in_buf = node->len - offset;
            
            if (rem_in_buf > 0) {
                if (amount == 0) {
                    return;
                }
                
                std::size_t take = MinValue(rem_in_buf, amount);

                func(node->ptr + offset, std::size_t(take));
                
                tot_len -= take;
                
                if (take < rem_in_buf || node->next == nullptr) {
                    offset += take;
                    AIPSTACK_ASSERT(amount == take);
                    return;
                }
                
                amount -= take;
            } else {
                if (node->next == nullptr) {
                    AIPSTACK_ASSERT(amount == 0);
                    return;
                }
            }
            
            node = node->next;
            offset = 0;
        }
    }
    
    /**
     * Process and consume up to a number of bytes from the front of the memory range
     * by invoking a callback function on contiguous chunks.
     * 
     * This is a more flexible variation of @ref processBytes which allows the callback
     * function to interrupt processing and to process fewer bytes than it was called
     * for. Additionally, it is permitted to pass `max_amount` greater than @ref tot_len in
     * order to simplify common use cases.
     * 
     * The `func` function will be called on the subsequent contiguous chunks of the
     * consumed part of this memory range. It will be passed two arguments: a char pointer
     * to the start of the chunk, and a size_t reference to the length of the chunk. It
     * must return boolean indicating whether to continue (false) or interrupt processing
     * (true). The function will not be called on zero-sized chunks. The function may
     * modify the chunk length via the passed reference to possibly report that it has
     * processed less bytes than it was provided with.
     * 
     * The effective number of bytes available for processing is min(`max_amount`,
     * @ref tot_len at the time of the call). If `func` never returns true, all of the
     * available bytes will have been processed. If `func` ever returns true, processing
     * stops that time, possibly (but not necessarily) with less than the available number
     * of bytes having been processed.
     * 
     * This function moves forward to subsequent buffers eagerly. This means that when no
     * more bytes will be processed (either because all available bytes have been processed
     * or because `func` returned true), it will move to the next buffer as long as it is
     * at the end of the current buffer and there is a next buffer. See @ref processBytes
     * for why this might be useful.
     * 
     * @tparam Func Function object type.
     * @param max_amount Maximum number of bytes to process and consume. In any case no more
     *        than @ref tot_len bytes will be processed.
     * @param func Function to call in order to process chunks (see above). The
     *        function must not modify this @ref IpBufRef object, and the state of
     *        this object at the time of invocation is unspecified.
     * @return False if processing was not interrupted (`func` never returned true and all
     *         available bytes have been processed), true if processing was interrupted
     *         (`func` returned true last time it was called, less than all available bytes
     *         may have been processed).
     */
    template<typename Func>
    bool processBytesInterruptible (std::size_t max_amount, Func func)
    {
        AIPSTACK_ASSERT(node != nullptr);
        
        std::size_t amount = MinValue(max_amount, tot_len);
        
        bool interrupted = false;

        while (true) {
            AIPSTACK_ASSERT(offset <= node->len);
            std::size_t rem_in_buf = node->len - offset;
            
            if (rem_in_buf > 0) {
                if (amount == 0) {
                    break;
                }
                
                std::size_t max_take = MinValue(rem_in_buf, amount);

                std::size_t take = max_take;
                interrupted = func(node->ptr + offset, static_cast<std::size_t &>(take));
                AIPSTACK_ASSERT(take <= max_take);

                tot_len -= take;
                amount -= take;
                
                if (interrupted) {
                    amount = 0;
                }
                
                if (take < rem_in_buf || node->next == nullptr) {
                    offset += take;
                    continue;
                }
            } else {
                if (node->next == nullptr) {
                    AIPSTACK_ASSERT(amount == 0);
                    break;
                }
            }
            
            node = node->next;
            offset = 0;
        }

        return interrupted;
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
     * Return a sub-range of the buffer reference from the given
     * offset of the given length.
     * 
     * This is implemented by calling @ref skipBytes(`offset_`) on a copy
     * of this object then returning @ref subTo(`len`) of this copy.
     * 
     * @param offset_ Offset from the start of this memory range. Must be less than
     *        or equal to @ref tot_len.
     * @param len Length of sub-range. Must be less than or equal to @ref tot_len
     *        - `offset_`.
     * @return The sub-range starting at `offset_` whose length is `len`.
     */
    inline IpBufRef subFromTo (std::size_t offset_, std::size_t len) const
    {
        IpBufRef buf = *this;
        buf.skipBytes(offset_);
        buf = buf.subTo(len);
        return buf;
    }

private:
    static void assertBufSanity (IpBufRef buf)
    {
        AIPSTACK_ASSERT(buf.node != nullptr);
        AIPSTACK_ASSERT(buf.offset <= buf.node->len);
    }
};

/** @} */

}

#endif
