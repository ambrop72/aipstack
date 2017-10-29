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

#ifndef AIPSTACK_TX_ALLOC_HELPER_H
#define AIPSTACK_TX_ALLOC_HELPER_H

#include <stddef.h>

#include <aipstack/misc/Assert.h>
#include <aipstack/infra/Buf.h>

namespace AIpStack {

/**
 * @addtogroup buffer
 * @{
 */

/**
 * Dummy class for the constructor
 * @ref TxAllocHelper::TxAllocHelper(TxAllocHelperUninitialized)
 * "TxAllocHelper(TxAllocHelperUninitialized)".
 */
class TxAllocHelperUninitialized {};

/**
 * Provides simple automatic/static allocation for outgoing packets.
 * 
 * This class is intended for temporary allocation of outgoing packets. Memory is allocated
 * as an array embedded in this object; when used as an automatic variable, this usually
 * results in stack allocation.
 * 
 * This class is used as follows:
 * 1. The constructor @ref TxAllocHelper(size_t) is used to construct for a specific data
 *    size. Alternatively, the constructor @ref TxAllocHelper(TxAllocHelperUninitialized)
 *    is used then @ref reset is called with the data size.
 * 2. @ref getPtr is called to get the pointer to the embedded buffer and data is written to
 *    this location. This can also be done later (this class never accesses the data only
 *    allocates the memory).
 * 3. Optionally, @ref changeSize is used to set a size possibly different from what was
 *    specified in step (1).
 * 4. If the outgoing packet is to contain additional data following the data in the
 *    embedded buffer, @ref setNext is used to link to the remainder of the data.
 * 5. @ref getBufRef is used to obtain an @ref IpBufRef referencing the entire data (that is
 *    the data within the embedded buffer possibly followed by the data linked to using
 *    @ref setNext).
 * 6. This @ref IpBufRef is passed to some kind of send function which reads the data.
 * 
 * @tparam MaxSize Maximum possible data size.
 * @tparam HeaderBefore Space before the data to reserve for headers.
 */
template <size_t MaxSize, size_t HeaderBefore>
class TxAllocHelper {
    static size_t const TotalMaxSize = HeaderBefore + MaxSize;
    
public:
    /**
     * Construct without a defined size.
     * 
     * This should only be used if one must construct the object before the data size is
     * known. In this case, the object is in a special uninitialized state and @ref reset
     * must be called before @ref changeSize, @ref setNext or @ref getBufRef.
     */
    inline TxAllocHelper (TxAllocHelperUninitialized)
    {
        m_node.ptr = m_data;
#if AIPSTACK_ASSERTIONS
        m_initialized = false;
#endif
    }
    
    /**
     * Construct for data of the specified size.
     * 
     * Note that it is still possible to subsequently change the size using
     * @ref changeSize.
     * 
     * @param size Data size. Must be less than or equal to `MaxSize`.
     */
    inline TxAllocHelper (size_t size)
    : TxAllocHelper(TxAllocHelperUninitialized())
    {
        reset(size);
    }
    
    /**
     * Reset the object for data of the specified size.
     * 
     * This is equivalent to reconstructing the object using the constructor
     * @ref TxAllocHelper(size_t).
     * 
     * @param size Data size. Must be less than or equal to `MaxSize`.
     */
    inline void reset (size_t size)
    {
        AIPSTACK_ASSERT(size <= MaxSize)
        AIPSTACK_ASSERT(m_node.ptr == m_data)
        
        m_node.len = HeaderBefore + size;
        m_node.next = nullptr;
        m_tot_len = size;
#if AIPSTACK_ASSERTIONS
        m_initialized = true;
#endif
    }
    
    /**
     * Get the pointer to the embedded buffer.
     * 
     * There are `MaxSize` bytes available at this location, and also `HeaderBefore` bytes
     * available before this location. This can be called at any time regardless of the
     * state of this object.
     * 
     * @return Pointer to embedded buffer, after any space reserved for headers.
     */
    inline char * getPtr ()
    {
        return m_data + HeaderBefore;
    }
    
    /**
     * Change the size of the data.
     * 
     * This can be called to change the data size after it has already been specified using
     * @ref TxAllocHelper(size_t) or @ref reset. It must not be called after @ref setNext
     * (unless @ref reset has subsequently been called).
     * 
     * @param size Data size. Must be less than or equal to `MaxSize`.
     */
    inline void changeSize (size_t size)
    {
        AIPSTACK_ASSERT(m_initialized)
        AIPSTACK_ASSERT(m_node.next == nullptr)
        AIPSTACK_ASSERT(size <= MaxSize)
        
        m_node.len = HeaderBefore + size;
        m_tot_len = size;
    }
    
    /**
     * Link to additional data that will follow data in the embedded buffer.
     * 
     * This can be used to add additional data after data in the embedded buffer. If this is
     * used, the `next` pointer in the first @ref IpBufNode of the buffer chain will point
     * to the specified `next_node`, and the specified `next_len` bytes will be included in
     * the `tot_len` of the @ref IpBufRef returned by @ref getBufRef.
     * 
     * Once this is called, it must not be called again and @ref changeSize must not be
     * called after this, until @ref reset is done.
     * 
     * @param next_node Pointer to the @ref IpBufNode where the additional data starts. Must
     *        not be null.
     * @param next_len Length of the additional data.
     */
    inline void setNext (IpBufNode const *next_node, size_t next_len)
    {
        AIPSTACK_ASSERT(m_initialized)
        AIPSTACK_ASSERT(m_node.next == nullptr)
        AIPSTACK_ASSERT(m_node.len == HeaderBefore + m_tot_len)
        AIPSTACK_ASSERT(next_node != nullptr)
        
        m_node.next = next_node;
        m_tot_len += next_len;
    }
    
    /**
     * Get an @ref IpBufRef referencing the embedded data and any additional data.
     * 
     * There will be `HeaderBefore` bytes of data available in the first buffer before
     * the actual data, for protocol headers.
     * 
     * This returns an @ref IpBufRef which has:
     * - @ref IpBufRef::node "node" pointing to an @ref IpBufNode object stored within this
     *   @ref TxAllocHelper object.
     * - @ref IpBufRef::offset "offset" equal to `HeaderBefore`.
     * - @ref IpBufRef::tot_len "tot_len" equal to the embedded data size as specified in
     *   @ref TxAllocHelper(size_t), @ref reset or @ref changeSize plus the size of any
     *   additional data specified by @ref setNext.
     * 
     * The internal @ref IpBufNode object which is the first in the buffer chain will have:
     * - @ref IpBufNode::ptr "ptr" equal to the start of the contained buffer including the
     *   header space, that is @ref getPtr() - `HeaderBefore`.
     * - @ref IpBufNode::len "len" equal to the embedded data size as specified in
     *   @ref TxAllocHelper(size_t), @ref reset or @ref changeSize.
     * - @ref IpBufNode::next "next" equal to null (if @ref setNext has not been used) or
     *        the `next_node` specified in @ref setNext (if it has been).
     * 
     * Note that a subsequent @ref reset would invalidate the returned reference as that
     * may modify the internal @ref IpBufNode object.
     * 
     * @return The @ref IpBufRef referencing the data.
     */
    inline IpBufRef getBufRef ()
    {
        AIPSTACK_ASSERT(m_initialized)
        
        return IpBufRef{&m_node, HeaderBefore, m_tot_len};
    }
    
private:
    IpBufNode m_node;
    size_t m_tot_len;
    char m_data[TotalMaxSize];
#if AIPSTACK_ASSERTIONS
    bool m_initialized;
#endif
};

/** @} */

}

#endif
