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

#ifndef AIPSTACK_CHKSUM_H
#define AIPSTACK_CHKSUM_H

#include <stdint.h>
#include <stddef.h>

#include <aipstack/meta/BasicMetaUtils.h>
#include <aipstack/misc/Assert.h>
#include <aipstack/misc/Hints.h>
#include <aipstack/misc/MinMax.h>
#include <aipstack/misc/BinaryTools.h>
#include <aipstack/infra/Buf.h>

/**
 * @ingroup infra
 * @defgroup checksum Checksum Calculation
 * @brief Provides utilities for calculating IP checksums.
 * 
 * For applications, the most important thing here is the \ref IpChksumInverted
 * function, for which an optimized implementation can be provided.
 */

#if defined(AIPSTACK_EXTERNAL_CHKSUM)
extern "C" uint16_t IpChksumInverted (char const *data, size_t len);
#else

/**
 * @ingroup checksum
 * Calculate the inverted IP checksum of a buffer.
 * 
 * This function calculates the inverted IP checksum of a contiguous sequence of
 * bytes. More specifically, it calculates the ones-complement sum of 16-bit words
 * as if those were represented in big-endian byte order (the result depends only
 * on the sequence of bytes and not the byte order of the processor). To obtain
 * an actual IP checksum for use in protocol headers, the result would need to be
 * bit-flipped (see @ref AIpStack::IpChksum "IpChksum").
 *
 * If the number of bytes is odd, this is treated as if there was an extra
 * zero byte at the end.
 * 
 * If the macro `AIPSTACK_EXTERNAL_CHKSUM` is defined, then only an `extern "C"`
 * function declaration is provided by the header file Chksum.h and the implementation
 * must be provided by the application.
 * 
 * @param data Pointer to data (must not be null).
 * @param len Number of bytes (may be zero). It must not exceed 65535 (this may
 *        allow a more optimized custom implementation).
 * @return Inverted IP checksum (ones-complement sum of 16-bit words).
 */
AIPSTACK_NO_INLINE
inline uint16_t IpChksumInverted (char const *data, size_t len)
{
    using namespace AIpStack;
    
    char const *even_end = data + (len & size_t(-2));
    uint32_t sum = 0;
    
    while (data < even_end) {
        sum += ReadBinaryInt<uint16_t, BinaryBigEndian>(data);
        data += 2;
    }
    
    if ((len & 1) != 0) {
        uint8_t byte = ReadBinaryInt<uint8_t, BinaryBigEndian>(data);
        sum += uint32_t(uint16_t(byte) << 8);
    }
    
    sum = (sum & UINT32_C(0xFFFF)) + (sum >> 16);
    sum = (sum & UINT32_C(0xFFFF)) + (sum >> 16);
    
    return uint16_t(sum);
}

#endif

namespace AIpStack {

/**
 * @addtogroup checksum
 * @{
 */

/**
 * Calculate the IP checksum of a buffer.
 * 
 * This function calculates the IP checksum of a contiguous sequence of bytes.
 * It is equivalent to calling @ref IpChksumInverted and bit-flipping the result
 * (in fact it is implemented like that). As such, the result is suitable for use
 * in protocol headers (assuming correct encoding/decoding to/from big-endian).
 * 
 * @param data Pointer to data (must not be null).
 * @param len Number of bytes (may be zero). It must not exceed 65535.
 * @return IP checksum.
 */
inline uint16_t IpChksum (char const *data, size_t len)
{
    return ~IpChksumInverted(data, len);
}

/**
 * Provides incremental IP checksum calculation of header words followed by data.
 * 
 * This class must be used according to the following pattern:
 * 1. Construct a new instance using the default constructor \ref IpChksumAccumulator().
 * 2. Call the following functions as needed to add the header to the running checksum:
 *    @ref addWord(WrapType<uint16_t>, uint16_t),
 *    @ref addWord(WrapType<uint32_t>, uint32_t),
 *    @ref addWords(WordType const *), @ref addWords(WordType const (*)[NumWords]),
 *    @ref addEvenBytes. The order of these calls with respect to each another does
 *    not matter due to commutativity of the IP checksum.
 * 3. Call @ref getChksum() or @ref getChksum(IpBufRef) to add any data to the
 *    running checksum (only in the latter case) and return the calculated checksum.
 * 
 * After calling any of the `getChksum` functions, the @ref IpChksumAccumulator object
 * is considered to be in an invalid state; subsequent calculations must be done with
 * newly constructed objects.
 * 
 * It is possible to export the state of the calculation by calling @ref getState and
 * later resume the calculation with a new object constructed using
 * @ref IpChksumAccumulator(State)
 */
class IpChksumAccumulator {
private:
    uint32_t m_sum;
    
public:
    /**
     * Data type representing the exported state of a checksum calculation.
     */
    enum State : uint32_t {};
    
    /**
     * Construct the object to start a new checksum calculation.
     */
    inline IpChksumAccumulator ()
    : m_sum(0)
    {
    }
    
    /**
     * Construct the object to resume a checksum calculation.
     * 
     * @param state The exported calculation state as returned by @ref getState.
     */
    inline IpChksumAccumulator (State state)
    : m_sum(state)
    {
    }
    
    /**
     * Export the state of the calculation for resuming later.
     * 
     * @return The exported calculation state.
     */
    inline State getState () const
    {
        return State(m_sum);
    }
    
    /**
     * Add a 16-bit word.
     * 
     * @param word The word to add.
     */
    inline void addWord (WrapType<uint16_t>, uint16_t word)
    {
        m_sum += word;
    }
    
    /**
     * Add a 32-bit word.
     * 
     * @param word The word to add.
     */
    inline void addWord (WrapType<uint32_t>, uint32_t word)
    {
        addWord(WrapType<uint16_t>(), uint16_t(word >> 16));
        addWord(WrapType<uint16_t>(), uint16_t(word));
    }
    
    /**
     * Add a constant number of contiguous words.
     * 
     * The word widths supported are those for which an `addWord` overloads exist.
     * 
     * @tparam WordType Unsigned integer type corresponding to the word width;
     *         see `addWord` overloads for supported widths/types.
     * @tparam NumWords Number of words (must be >=0).
     * @param words Pointer to the words to add.
     */
    template <typename WordType, int NumWords>
    inline void addWords (WordType const *words)
    {
        for (int i = 0; i < NumWords; i++) {
            addWord(WrapType<WordType>(), words[i]);
        }
    }
    
    /**
     * Add a constant-sized array of words.
     * 
     * This is equivalent to @ref addWords(WordType const *) but can deduce the number
     * of words based on the array type.
     * 
     * @tparam WordType Unsigned integer type corresponding to the word width;
     *         see `addWord` overloads for supported widths/types.
     * @tparam NumWords Number of words (must be >=0).
     * @param words Pointer to the array of words to add.
     */
    template <typename WordType, int NumWords>
    inline void addWords (WordType const (*words)[NumWords])
    {
        addWords<WordType, NumWords>(*words);
    }
    
    /**
     * Add an even number of contiguous bytes.
     * 
     * @param ptr Pointer to data (must not be null).
     * @param num_bytes Number of bytes (must be even, may be zero).
     */
    inline void addEvenBytes (char const *ptr, size_t num_bytes)
    {
        AIPSTACK_ASSERT(num_bytes % 2 == 0)
        
        char const *endptr = ptr + num_bytes;
        while (ptr < endptr) {
            uint16_t word = ReadBinaryInt<uint16_t, BinaryBigEndian>(ptr);
            ptr += 2;
            addWord(WrapType<uint16_t>(), word);
        }
    }
    
    /**
     * Complete and return the checksum without adding any additional data.
     * 
     * After this function is called, the @ref IpChksumAccumulator object is considered
     * to be in an invalid state and its further use would have unspecified results.
     * 
     * @return The calculated checksum.
     */
    inline uint16_t getChksum ()
    {
        foldOnce();
        foldOnce();
        return uint16_t(~m_sum);
    }
    
    /**
     * Add the data referenced by @ref IpBufRef then complete and return the checksum.
     * 
     * After this function is called, the @ref IpChksumAccumulator object is considered
     * to be in an invalid state and its further use would have unspecified results.
     * 
     * @param buf Reference to the sequence of data bytes to add before completing
     *        the checksum. Its length (`buf.tot_len`) may be any number including
     *        zero (if zero, then `buf.node` is not examined and may be null).
     * @return The calculated checksum.
     */
    inline uint16_t getChksum (IpBufRef buf)
    {
        if (buf.tot_len > 0) {
            addIpBuf(buf);
        }
        return getChksum();
    }
    
private:
    inline void foldOnce ()
    {
        m_sum = (m_sum & TypeMax<uint16_t>()) + (m_sum >> 16);
    }
    
    inline static uint32_t swapBytes (uint32_t x)
    {
        return ((x >> 8) & UINT32_C(0x00FF00FF)) | ((x << 8) & UINT32_C(0xFF00FF00));
    }
    
    void addIpBuf (IpBufRef buf)
    {
        bool swapped = false;
        
        do {
            size_t len = buf.getChunkLength();
            
            // Calculate sum of buffer.
            uint16_t buf_sum = IpChksumInverted(buf.getChunkPtr(), len);
            
            // Add the buffer sum to our sum.
            uint32_t old_sum = m_sum;
            m_sum += buf_sum;
            
            // Fold back any overflow.
            if (AIPSTACK_UNLIKELY(m_sum < old_sum)) {
                m_sum++;
            }
            
            // If the buffer has an odd length, swap bytes in sum.
            if (len % 2 != 0) {
                m_sum = swapBytes(m_sum);
                swapped = !swapped;
            }
        } while (buf.nextChunk());
        
        // Swap bytes if we swapped an odd number of times.
        if (swapped) {
            m_sum = swapBytes(m_sum);
        }
    }
};

/**
 * Calculate the IP checksum of a sequence of bytes described by @ref IpBufRef.
 * 
 * This function calculates the IP checksum of a possibly discontiguous sequence
 * of bytes. It is functionally equivalent to
 * @ref IpChksum(char const *, size_t) except that the sequence of bytes is specified
 * using @ref IpBufRef.
 * 
 * This function is implemented by default-constructing an @ref IpChksumAccumulator
 * then calling @ref IpChksumAccumulator::getChksum(IpBufRef).
 * 
 * @param buf Reference to the sequence of bytes.
 * @return IP checksum.
 */
inline uint16_t IpChksum (IpBufRef buf)
{
    IpChksumAccumulator accum;
    return accum.getChksum(buf);
}

/** @} */

}

#endif
