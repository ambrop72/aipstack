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

#ifndef AIPSTACK_MODULO_H
#define AIPSTACK_MODULO_H

#include <stddef.h>

#include <aipstack/misc/Assert.h>
#include <aipstack/misc/MinMax.h>

namespace AIpStack {

/**
 * @ingroup misc
 * @defgroup modulo Modular Arithmetic Utilities
 * @brief Utilities for more easily using modular arithemtic.
 * 
 * The class @ref Modulo represents a modulus value and provides functions for performing
 * arithmetic using that modulus. The functions @ref visitModuloRange and
 * @ref visitModuloRange2 can be used to visit contiguous subranges within a range or two
 * ranges respectively.
 * 
 * @{
 */

/**
 * Provides modular arithemtic operations using a specific modulus.
 */
class Modulo
{
private:
    size_t const m_modulus;
    
public:
    /**
     * Construct for the given modulus.
     * 
     * @param modulus The modulus to use (must be greater than zero).
     */
    inline constexpr Modulo (size_t modulus) :
        m_modulus(modulus)
    {}
    
    /**
     * Return the modulus.
     * 
     * @return The modulus.
     */
    inline constexpr size_t modulus () const
    {
        return m_modulus;
    }
    
    /**
     * Add integers modulo.
     * 
     * Computes ((a + b) mod modulus), provided that a and b are both less than or equal
     * to the modulus and not both equal to the modulus. If these preconditions are not
     * satisifed, the result is undefined.
     * 
     * @param a First operand.
     * @param b Second operand.
     * @return (a + b) mod modulus, assuming the preconditions are satisifed.
     */
    inline constexpr size_t add (size_t a, size_t b) const
    {
        return (b < m_modulus - a) ? (a + b) : (b - (m_modulus - a));
    }
    
    /**
     * Subtract integers modulo.
     * 
     * Computes ((a - b) mod modulus), provided that a is less than the modulus and b is
     * less than or equal to the modulus. If these preconditions are not satisifed, the
     * result is undefined.
     * 
     * @param a First operand.
     * @param b Second operand.
     * @return (a - b) mod modulus, assuming the preconditions are satisifed.
     */
    inline constexpr size_t sub (size_t a, size_t b) const
    {
        return (b <= a) ? (a - b) : (m_modulus - (b - a));
    }
    
    /**
     * Increment an integer by one modulo.
     * 
     * Computes ((a + 1) mod modulus), provided that a is less than the modulus. If this
     * precondition is not satisifed, the result is undefined.
     * 
     * @param a Operand.
     * @return (a + 1) mod modulus, assuming the preconditions are satisifed.
     */
    inline constexpr size_t inc (size_t a) const
    {
        size_t r = a + 1;
        if (r == m_modulus) {
            r = 0;
        }
        return r;
    }
    
    /**
     * Return the modulus minus the operand.
     * 
     * Computes (modulus - a), provided that a is less than or equal to the modulus.
     * If this precondition is not satisifed, the result is undefined.
     * 
     * This is NOT the same as (-a mod modulus) because when a is 0, the result of
     * this function is the modulus rather than 0. This function is named "modulus
     * complement" because adding 'a' and the result gives the modulus.
     * 
     * For a circular buffer, this calculates the amount of free space given the amount
     * of used space or the converse, and also calculates the distance from a position
     * in the buffer to the end (the wrap-around point).
     * 
     * @param a Operand.
     * @return (modulus - a), assuming the preconditions are satisifed.
     */
    inline constexpr size_t modulusComplement (size_t a) const
    {
        return m_modulus - a;
    }
};

/**
 * Visit contiguous sub-ranges of a modular arithmetic range (e.g.\ a range in a
 * circular buffer).
 * 
 * This calls the given 'visit' function object for each contiguous sub-range of the
 * given range. The 'visit' object is called as `visit(rel_pos, subrange_pos, subrange_len)`
 * where the arguments are size_t values specifying the relative offset in the complete
 * range, the modular start position of the sub-range and the length of the sub-range
 * respecitvely.
 * 
 * The 'visit' object is called for sub-ranges in the natural order and never for an
 * empty sub-range.
 * 
 * If you need to synchronously process different ranges (for example to copy from
 * one circular buffer to another), use @ref visitModuloRange2 instead of this.
 * 
 * @param mod The modulus (e.g. the size of a circular buffer).
 * @param pos Starting position of the range to process. Must be less than the modulus.
 * @param count Length of the range to process.
 * @param visit Function object to call for each contiguous sub-range.
 */
template <typename Visit>
void visitModuloRange (Modulo mod, size_t pos, size_t count, Visit visit)
{
    AIPSTACK_ASSERT(pos < mod.modulus())
    
    size_t rel_pos = 0;
    
    while (count > 0) {
        size_t chunk_len = MinValue(count, mod.modulusComplement(pos));
        
        visit(size_t(rel_pos), size_t(pos), size_t(chunk_len));
        
        pos = mod.add(pos, chunk_len);
        count -= chunk_len;
        rel_pos += chunk_len;
    }
}

/**
 * Visit common contiguous sub-ranges of two modular arithmetic ranges (e.g.\ ranges
 * in circular buffers).
 * 
 * This is the equivalent of @ref visitModuloRange for synchronized processing
 * of two ranges using generally different moduli. For example, this can be used
 * to copy data from one circular buffer to another.
 * 
 * The 'visit' function object is called as
 * `visit(subrange_pos1, subrange_pos2, subrange_len)` where the arguments are
 * size_t values specifying the start position for the first modulus, the start
 * position for the second modulus, and the common subrange length respectively.
 * 
 * The 'visit' object is called for sub-ranges in the natural order and never
 * for an empty sub-range.
 * 
 * @param mod1 The first modulus.
 * @param pos1 Starting position for the first modulus. Must be less than 'mod1'.
 * @param mod2 The second modulus.
 * @param pos2 Starting position for the second modulus. Must be less than 'mod2'.
 * @param count Common length of the two ranges to process.
 * @param visit Function object to call for each common contiguous sub-range.
 */
template <typename Visit>
void visitModuloRange2 (Modulo mod1, size_t pos1, Modulo mod2, size_t pos2,
                        size_t count, Visit visit)
{
    AIPSTACK_ASSERT(pos1 < mod1.modulus())
    AIPSTACK_ASSERT(pos2 < mod2.modulus())
    
    while (count > 0) {
        size_t chunk_len = MinValue(count, MinValue(mod1.modulusComplement(pos1),
                                                    mod2.modulusComplement(pos2)));
        
        visit(size_t(pos1), size_t(pos2), size_t(chunk_len));
        
        pos1 = mod1.add(pos1, chunk_len);
        pos2 = mod2.add(pos2, chunk_len);
        count -= chunk_len;
    }
}

/** @} */

}

#endif
