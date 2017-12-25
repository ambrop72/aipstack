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

#ifndef AIPSTACK_MEMREF_H
#define AIPSTACK_MEMREF_H

#include <stddef.h>
#include <string.h>

#include <aipstack/misc/Assert.h>

namespace AIpStack {

/**
 * @addtogroup infra
 * @{
 */

/**
 * References a contiguous byte sequence.
 * 
 * A @ref MemRef structure is defined by a pointer to the start of the
 * byte sequence (@ref ptr) and the number of bytes (@ref len).
 */
struct MemRef {
    /**
     * Pointer to the start of the byte sequence.
     * 
     * For a valid @ref MemRef (one that references a byte sequence), if @ref len is nonzero
     * then @ref ptr must be non-null and must point to a valid buffer with at least
     * @ref len bytes available. Whether @ref ptr may be null if @ref len is zero depends
     * on the requirements of the operation or whatever the @ref MemRef is passed to.
     */
    char const *ptr;
    
    /**
     * Number of bytes in the byte sequence.
     */
    size_t len;
    
    /**
     * Default constructor, sets null pointer and zero length (same as @ref Null).
     */
    inline MemRef () :
        ptr(nullptr),
        len(0)
    {}
    
    /**
     * Construct from pointer and length.
     * 
     * @param ptr_arg Value to initialize @ref ptr with.
     * @param len_arg Value to initialize @ref len with.
     */
    inline MemRef (char const *ptr_arg, size_t len_arg)
    : ptr(ptr_arg), len(len_arg)
    {
    }
    
    /**
     * Construct from null-terminated string.
     * 
     * This initializes @ref ptr to `cstr` and @ref len to `strlen(cstr)`.
     * 
     * @param cstr Pointer to null-terminated string (must not be null).
     */
    inline MemRef (char const *cstr)
    : ptr(cstr), len(strlen(cstr))
    {
    }
    
    /**
     * Return a @ref MemRef with null pointer and zero length.
     * 
     * @return a @ref MemRef with null @ref ptr and zero @ref len.
     */
    inline static MemRef Null ()
    {
        return MemRef(nullptr, 0);
    }
    
    /**
     * Return the byte at the given position.
     * 
     * @param pos Position of the byte. Must be less than @ref len.
     * @return `ptr[pos]`
     */
    inline char at (size_t pos) const
    {
        AIPSTACK_ASSERT(ptr)
        AIPSTACK_ASSERT(pos < len)
        
        return ptr[pos];
    }
    
    /**
     * Return a @ref MemRef referencing a suffix of this byte sequence.
     * 
     * @ref ptr must be non-null, even if @ref len is zero.
     * 
     * @param offset Offset where the suffix byte sequence is to start.
     *        Must be less than or equal to @ref len.
     * @return A @ref MemRef with @ref ptr incremented by `offset` and @ref len
     *         decremented by `offset` compared to this object.
     */
    inline MemRef subFrom (size_t offset) const
    {
        AIPSTACK_ASSERT(ptr)
        AIPSTACK_ASSERT(offset <= len)
        
        return MemRef(ptr + offset, len - offset);
    }
    
    /**
     * Return a @ref MemRef referencing a prefix of this byte sequence.
     * 
     * @ref ptr must be non-null, even if @ref len is zero.
     * 
     * @param offset Offset where the prefix byte sequence is to end.
     *        Must be less than or equal to @ref len.
     * @return a @ref MemRef with @ref ptr the same as this object and @ref len equal to
     *         `offset`.
     */
    inline MemRef subTo (size_t offset) const
    {
        AIPSTACK_ASSERT(ptr)
        AIPSTACK_ASSERT(offset <= len)
        
        return MemRef(ptr, offset);
    }
    
    /**
     * Compare the byte sequences referenced by this @ref MemRef and another.
     * 
     * @ref ptr must be null for both @ref MemRef, even if the corresponding @ref len is
     * zero.
     * 
     * @param other The other @ref MemRef to compare.
     * @return True if both byte sequences have the same length and contents, false
     *         otherwise.
     */
    inline bool equalTo (MemRef other) const
    {
        AIPSTACK_ASSERT(ptr)
        AIPSTACK_ASSERT(other.ptr)
        
        return len == other.len && !memcmp(ptr, other.ptr, len);
    }
    
    /**
     * Check if this byte sequence starts with the given prefix and adjust this
     * @ref MemRef over the prefix if it does.
     * 
     * If the byte sequence has the given prefix, this @ref MemRef is updated to
     * reference the remaining part after the prefix.
     * 
     * @ref ptr must be non-null, even if @ref len is zero.
     * 
     * @param prefix Prefix to check for as a null-terminated string (must not be null).
     * @return True if the prefix was found (this object has been updated), false if
     *         the prefix was not found (this object has not been changed).
     */
    bool removePrefix (char const *prefix)
    {
        size_t pos = 0;
        while (prefix[pos] != '\0') {
            if (pos == len || ptr[pos] != prefix[pos]) {
                return false;
            }
            pos++;
        }
        *this = subFrom(pos);
        return true;
    }
};

/** @} */

}

#endif
