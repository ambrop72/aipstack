/*
 * Copyright (c) 2019 Ambroz Bizjak
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

#ifndef AIPSTACK_BUF_UTILS_H
#define AIPSTACK_BUF_UTILS_H

#include <cstddef>
#include <cstring>

#include <aipstack/misc/Assert.h>
#include <aipstack/misc/MinMax.h>
#include <aipstack/misc/MemRef.h>
#include <aipstack/misc/TypedFunction.h>
#include <aipstack/infra/Buf.h>

namespace AIpStack {
/**
 * @addtogroup buffer
 * @{
 */

/**
 * Return an @ref IpBufNode corresponding to the first buffer of the memory range
 * with the offset applied.
 * 
 * @param buf Buffer to work with.
 * @return An @ref IpBufNode with `ptr` equal to `buf.node->ptr + buf.offset`,
 *        `len` equal to `buf.node->len - buf.offset` and `next` equal to
 *        `buf.node->next`.
 */
inline IpBufNode ipBufRefToNode (IpBufRef buf)
{
    IpBufRef::assertBufSanity(buf);
    
    return IpBufNode {
        buf.node->ptr + buf.offset,
        std::size_t(buf.node->len - buf.offset),
        buf.node->next
    };
}

/**
 * Construct a memory range consisting of an initial portion of the first chunk of
 * a memory range continued by data in a specified buffer chain.
 * 
 * It sets `*out_node` to a new @ref IpBufNode referencing the initial portion and
 * continuing into the given buffer chain (`ptr = buf.node->ptr`,
 * `len = buf.offset + header_len`, `next = cont`), and returns an @ref IpBufRef
 * using `out_node` as its first buffer (`node = out_node`, `offset = buf.offset`,
 * `tot_len = total_len`).
 * 
 * Note that this does not "apply" the offset to the node as @ref ipBufRefToNode
 * does. This is to allow @ref IpBufRef::revealHeader.
 *
 * It is important to understand that this works by creating a new @ref IpBufNode,
 * because the buffer chain model cannot support this operation otherwise. The
 * returned @ref IpBufRef will be valid only so long as `out_node` remains valid.
 * 
 * @param buf Buffer where the initial portion originates from.
 * @param header_len Length of the initial portion. Must be less than or
 *        equal to `buf.node->len - buf.offset`.
 * @param cont Pointer to the buffer node with data after the initial
 *        portion (may be null if there is no such data).
 * @param total_len Total length of the constructed memory range.
 *        Must be greater than or equal to `header_len`.
 * @param out_node Pointer to where the new @ref IpBufNode for the
 *        initial portion will be written (must not be null).
 * @return An @ref IpBufRef referencing the constructed memory range.
 */
inline IpBufRef ipBufHeaderPrefixContinuedBy (IpBufRef buf, std::size_t header_len,
    IpBufNode const *cont, std::size_t total_len, IpBufNode *out_node)
{
    IpBufRef::assertBufSanity(buf);
    AIPSTACK_ASSERT(header_len <= buf.node->len - buf.offset);
    AIPSTACK_ASSERT(total_len >= header_len);
    
    *out_node = IpBufNode{buf.node->ptr, std::size_t(buf.offset + header_len), cont};
    return IpBufRef{out_node, buf.offset, total_len};
}

/**
 * Process and consume a number of bytes from the front of a memory range by
 * invoking a callback function on contiguous chunks.
 * 
 * The `processChunk` function will be called on the subsequent contiguous chunks of
 * the consumed part of this memory range. It will be passed two arguments: a char
 * pointer to the start of the chunk, and a size_t length of the chunk. The function
 * will not be called on zero-sized chunks.
 * 
 * This function moves forward to subsequent buffers eagerly. This means that when
 * there are no more bytes to be processed, it will move to the next buffer as long
 * as it is at the end of the current buffer and there is a next buffer.
 * 
 * This eager moving across buffer is useful when the buffer chain is a ring buffer,
 * so that the offset into the buffer will remain always less than the buffer size,
 * never becoming equal.
 * 
 * @tparam FuncImpl Type of function object wrapped by `processChunk`.
 * @param buf Buffer to start with.
 * @param amount Number of bytes to process and consume. Must be less than
 *        or equal to `buf.tot_len`.
 * @param processChunk Function to call in order to process chunks (see above).
 * @return Updated buffer after processing.
 */
template<typename FuncImpl>
IpBufRef ipBufProcessBytes (IpBufRef buf, std::size_t amount,
    TypedFunction<void(char *, std::size_t), FuncImpl> processChunk)
{
    AIPSTACK_ASSERT(buf.node != nullptr);
    AIPSTACK_ASSERT(amount <= buf.tot_len);
    
    while (true) {
        AIPSTACK_ASSERT(buf.offset <= buf.node->len);
        std::size_t rem_in_buf = buf.node->len - buf.offset;
        
        if (rem_in_buf > 0) {
            if (amount == 0) {
                break;
            }
            
            std::size_t take = MinValue(rem_in_buf, amount);

            processChunk(buf.node->ptr + buf.offset, take);
            
            buf.tot_len -= take;
            
            if (take < rem_in_buf || buf.node->next == nullptr) {
                buf.offset += take;
                AIPSTACK_ASSERT(amount == take);
                break;
            }
            
            amount -= take;
        } else {
            if (buf.node->next == nullptr) {
                AIPSTACK_ASSERT(amount == 0);
                break;
            }
        }
        
        buf.node = buf.node->next;
        buf.offset = 0;
    }

    return buf;
}

/**
 * Structure returned by @ref ipBufProcessBytesInt.
 */
struct IpBufProcessIntResult {
    /**
     * The buffer after processing.
     */
    IpBufRef buf;

    /**
     * Whether processing was interrupted.
     * 
     * False means that `func` never returned true and all available bytes have been
     * processed. True means that processing was interrupted `func` returned true last
     * time it was called, less than all available bytes may have been processed.
     */
    bool interrupted;
};

/**
 * Process and consume up to a number of bytes from the front of the memory range
 * by invoking a callback function on contiguous chunks.
 * 
 * This is a more flexible variation of @ref ipBufProcessBytes which allows the callback
 * function to interrupt processing and to process fewer bytes than it was called
 * for. Additionally, it is permitted to pass `max_amount` greater than `buf.tot_len` in
 * order to simplify common use cases.
 * 
 * The `processChunk` function will be called on the subsequent contiguous chunks of
 * the consumed part of this memory range. It will be passed two arguments: a char
 * pointer to the start of the chunk, and a size_t reference to the length of the chunk.
 * It must return a boolean indicating whether to interrupt processing (true to
 * interrupt, false to continue). The function will not be called on zero-sized chunks.
 * The function may modify the chunk length via the passed reference to possibly report
 * that it has processed less bytes than it was provided with.
 * 
 * The effective number of bytes available for processing is min(`max_amount`,
 * `buf.tot_len`). If `processChunk` never returns true, all of the available bytes will
 * have been processed. If `processChunk` ever returns true, processing stops that time,
 * possibly (but not necessarily) with less than the available number of bytes having
 * been processed.
 * 
 * This function moves forward to subsequent buffers eagerly. This means that when no
 * more bytes will be processed (either because all available bytes have been processed
 * or because `func` returned true), it will move to the next buffer as long as it is
 * at the end of the current buffer and there is a next buffer. See @ref ipBufProcessBytes
 * for why this might be useful.
 * 
 * @tparam FuncImpl Type of function object wrapped by `processChunk`.
 * @param buf Buffer to start with.
 * @param max_amount Maximum number of bytes to process and consume. In any case no more
 *        than `buf.tot_len` bytes will be processed.
 * @param processChunk Function to call in order to process chunks (see above).
 * @return The updated buffer and whether processing was interrupted, encapsulated in a
 *         @ref IpBufProcessIntResult structure (see the struct description).
 */
template<typename FuncImpl>
IpBufProcessIntResult ipBufProcessBytesInt (IpBufRef buf, std::size_t max_amount,
    TypedFunction<bool(char *, std::size_t &), FuncImpl> processChunk)
{
    AIPSTACK_ASSERT(buf.node != nullptr);
    
    std::size_t amount = MinValue(max_amount, buf.tot_len);
    
    bool interrupted = false;

    while (true) {
        AIPSTACK_ASSERT(buf.offset <= buf.node->len);
        std::size_t rem_in_buf = buf.node->len - buf.offset;
        
        if (rem_in_buf > 0) {
            if (amount == 0) {
                break;
            }
            
            std::size_t max_take = MinValue(rem_in_buf, amount);

            std::size_t take = max_take;
            interrupted = processChunk(buf.node->ptr + buf.offset, take);
            AIPSTACK_ASSERT(take <= max_take);

            buf.tot_len -= take;
            amount -= take;
            
            if (interrupted) {
                amount = 0;
            }
            
            if (take < rem_in_buf || buf.node->next == nullptr) {
                buf.offset += take;
                continue;
            }
        } else {
            if (buf.node->next == nullptr) {
                AIPSTACK_ASSERT(amount == 0);
                break;
            }
        }
        
        buf.node = buf.node->next;
        buf.offset = 0;
    }

    return IpBufProcessIntResult{buf, interrupted};
}

/**
 * Consume a number of bytes from the front of the memory range.
 * 
 * This moves to subsequent buffers eagerly (see @ref ipBufProcessBytes).
 * 
 * @param buf Buffer to start with.
 * @param amount Number of bytes to consume. Must be less than or equal
 *        to `buf.tot_len`.
 * @return Updated buffer after processing.
 */
inline IpBufRef ipBufSkipBytes (IpBufRef buf, std::size_t amount)
{
    return ipBufProcessBytes(buf, amount, TypedFunction(
        [](char *, std::size_t) {
            // Nothing.
        }));
}

/**
 * Consume a number of bytes from the front of the memory memory range while copying
 * them to the given memory location.
 * 
 * This moves to subsequent buffers eagerly (see @ref ipBufProcessBytes).
 * 
 * @param buf Buffer to start with.
 * @param amount Number of bytes to copy out and consume. Must be less than
 *        or equal to `buf.tot_len`.
 * @param dst Location to copy to. May be null only if `amount` is zero.
 * @return Updated buffer after processing.
 */
inline IpBufRef ipBufTakeBytes (IpBufRef buf, std::size_t amount, char *dst)
{
    return ipBufProcessBytes(buf, amount, TypedFunction(
        [&](char *data, std::size_t len) {
            std::memcpy(dst, data, len);
            dst += len;
        }));
}

/**
 * Consume a number of bytes from the front of the memory range while copying bytes
 * from the given memory location into the consumed part of the range.
 * 
 * This moves to subsequent buffers eagerly (see @ref ipBufProcessBytes).
 * 
 * @param buf Buffer to start with.
 * @param data Reference to bytes to copy in (as @ref MemRef). `data.len` must be less
 *        than or equal to `buf.tot_len`. `data.ptr` may be null if `data.len` is zero.
 * @return Updated buffer after processing.
 */
inline IpBufRef ipBufGiveBytes (IpBufRef buf, MemRef data)
{
    char const *src = data.ptr;
    return ipBufProcessBytes(buf, data.len, TypedFunction(
        [&](char *cdata, std::size_t clen) {
            std::memcpy(cdata, src, clen);
            src += clen;
        }));
}

/**
 * Consume a number of bytes from the front of the memory range while copying bytes
 * from another memory range into the consumed part of the range.
 * 
 * The number of bytes consumed and copied is equal to the length of the other memory
 * range (`src`), and must not exceed the length of this memory range.
 * 
 * This moves to subsequent buffers eagerly (see @ref ipBufProcessBytes).
 * 
 * @param buf Buffer to start with.
 * @param src Memory range to copy in. `src.tot_len` must be less than
 *        or equal to `buf.tot_len` of this memory range.
 * @return Updated buffer after processing.
 */
inline IpBufRef ipBufGiveBuf (IpBufRef buf, IpBufRef src)
{
    return ipBufProcessBytes(buf, src.tot_len, TypedFunction(
        [&](char *data, std::size_t len) {
            src = ipBufTakeBytes(src, len, data);
        }));
}

/**
 * Get and consume a single byte from the front of the memory range.
 * 
 * `buf.tot_len` must be positive.
 * 
 * @note This function updates the buffer which is passed by reference.
 * 
 * This moves to subsequent buffers eagerly (see @ref ipBufProcessBytes).
 * 
 * @param buf Buffer to start with and update.
 * @return The value of the consumed byte.
 */
inline char ipBufTakeByteMut (IpBufRef &buf)
{
    AIPSTACK_ASSERT(buf.tot_len > 0);
    
    char byteVal = 0;

    buf = ipBufProcessBytes(buf, 1, TypedFunction(
        [&](char *data, std::size_t) {
            byteVal = *data;
        }));

    return byteVal;
}

/**
 * Set a number of bytes at the front of the memory range to a specific value and
 * consume them.
 * 
 * This moves to subsequent buffers eagerly (see @ref ipBufProcessBytes).
 * 
 * @param buf Buffer to start with.
 * @param byte Value to set bytes to.
 * @param amount Number of bytes to set and consume. Must be less than or equal to
 *        `buf.tot_len`.
 * @return Updated buffer after processing.
 */
inline IpBufRef ipBufGiveSameBytes (IpBufRef buf, char byte, std::size_t amount)
{
    return ipBufProcessBytes(buf, amount, TypedFunction(
        [&](char *data, std::size_t len) {
            std::memset(data, byte, len);
        }));
}

/**
 * Search for the first occurrence of the specified byte while consuming bytes from
 * the front of the memory range.
 *
 * If `byte` is found within the first `amount` bytes, all bytes up to and including
 * the first occurrence have been consumed. Otherwise, all bytes in the buffer have
 * been consumed.
 * 
 * @note This function updates the buffer which is passed by reference.
 * 
 * This moves to subsequent buffers eagerly (see @ref ipBufProcessBytesInt).
 * 
 * @param buf Buffer to start with and update.
 * @param byte Byte to search for.
 * @param amount Maximum number of bytes to process and consume. In any case no more
 *        than `buf.tot_len` bytes will be processed.
 * @return Whether the byte was found.
 */
inline bool ipBufFindByteMut (
    IpBufRef &buf, char byte, std::size_t amount = std::size_t(-1))
{
    IpBufProcessIntResult processRes = ipBufProcessBytesInt(buf, amount, TypedFunction(
        [&](char *data, std::size_t &len)
    {
        void *ch = std::memchr(data, byte, len);
        if (ch == nullptr) {
            return false;
        } else {
            len = std::size_t((reinterpret_cast<char *>(ch) - data) + 1);
            return true;
        }
    }));

    buf = processRes.buf;
    return processRes.interrupted;
}

/**
 * Check for and consume a specific prefix at the front of the memory range.
 * 
 * If the prefix is found, the remaining buffer is returned via the `remainder`
 * output parameter.
 * 
 * This moves to subsequent buffers eagerly (see @ref ipBufProcessBytesInt),
 * with respect to the returned `remainder`.
 * 
 * @param buf Buffer to start with.
 * @param prefix Prefix to check for (`prefix.ptr` may be null if `prefix.len` is 0).
 * @param remainder If the prefix is found, is set to a reference to the remainder of
 *        this memory range following the prefix (not modified if the prefix is not
 *        found).
 * @return True if the prefix is found, false if not.
 */
inline bool ipBufStartsWith (IpBufRef buf, MemRef prefix, IpBufRef &remainder)
{
    if (prefix.len > buf.tot_len) {
        return false;
    }

    std::size_t position = 0;

    IpBufProcessIntResult processResult = ipBufProcessBytesInt(buf, prefix.len,
        TypedFunction(
        [&](char *data, std::size_t &len) {
            if (std::memcmp(data, prefix.ptr + position, len) != 0) {
                return true;
            }
            position += len;
            return false;
        }));

    if (processResult.interrupted) {
        return false;
    }

    AIPSTACK_ASSERT(position == prefix.len);
    AIPSTACK_ASSERT(processResult.buf.tot_len == buf.tot_len - prefix.len);

    remainder = processResult.buf;
    return true;
}

/**
 * Return a sub-range of the buffer reference from the given offset of the given
 * length.
 * 
 * This is implemented by calling @ref ipBufSkipBytes followed by @ref IpBufRef::subTo.
 * 
 * @param buf Buffer to start with.
 * @param offset Offset from the start of this memory range. Must be less than
 *        or equal to `buf.tot_len`.
 * @param len Length of sub-range. Must be less than or equal to
 *        `buf.tot_len - offset`.
 * @return The sub-range starting at `offset` whose length is `len`.
 */
inline IpBufRef ip4BufSubFromTo (IpBufRef buf, std::size_t offset, std::size_t len)
{
    buf = ipBufSkipBytes(buf, offset);
    return buf.subTo(len);
}

/** @} */
}

#endif
