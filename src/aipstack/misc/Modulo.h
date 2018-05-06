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

#include <cstddef>

#include <aipstack/misc/Assert.h>
#include <aipstack/misc/MinMax.h>

namespace AIpStack {

/**
 * @addtogroup misc
 * @{
 */

/**
 * Represents a modulus value and provides modular arithemtic operations.
 */
class Modulo
{
private:
    std::size_t const m_modulus;
    
public:
    /**
     * Construct for the given modulus.
     * 
     * @param modulus The modulus to use (must be greater than zero).
     */
    inline constexpr Modulo (std::size_t modulus) :
        m_modulus(modulus)
    {}
    
    /**
     * Return the modulus.
     * 
     * @return The modulus.
     */
    inline constexpr std::size_t modulus () const
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
    inline constexpr std::size_t add (std::size_t a, std::size_t b) const
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
    inline constexpr std::size_t sub (std::size_t a, std::size_t b) const
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
    inline constexpr std::size_t inc (std::size_t a) const
    {
        std::size_t r = a + 1;
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
    inline constexpr std::size_t modulusComplement (std::size_t a) const
    {
        return m_modulus - a;
    }
};

/** @} */

}

#endif
