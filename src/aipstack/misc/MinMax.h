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

#ifndef AIPSTACK_MIN_MAX_H
#define AIPSTACK_MIN_MAX_H

#include <type_traits>
#include <limits>

namespace AIpStack {

/**
 * @ingroup misc
 * @defgroup min-max Minimum and Maximum Utilities
 * @brief Simple binary min, max and related functions.
 * 
 * @{
 */

/**
 * Return the smaller of two numbers (typically integers).
 * 
 * @tparam T Type of operand.
 * @param op1 First operand.
 * @param op2 Second operand.
 * @return `(op1 < op2) ? op1 : op2`
 */
template <typename T>
constexpr T MinValue (T op1, T op2)
{
    return (op1 < op2) ? op1 : op2;
}

/**
 * Return the larger of two numbers (typically integers).
 * 
 * @tparam T Type of operand.
 * @param op1 First operand.
 * @param op2 Second operand.
 * @return `(op1 > op2) ? op1 : op2`
 */
template <typename T>
constexpr T MaxValue (T op1, T op2)
{
    return (op1 > op2) ? op1 : op2;
}

/**
 * Return the absolute difference of two numbers (typically integers).
 * 
 * @tparam T Type of operand.
 * @param op1 First operand.
 * @param op2 Second operand.
 * @return `(op1 > op2) ? (op1 - op2) : (op2 - op1)`
 */
template <typename T>
constexpr T AbsoluteDiff (T op1, T op2)
{
    return (op1 > op2) ? (op1 - op2) : (op2 - op1);
}

#ifndef IN_DOXYGEN
template <typename T1, typename T2>
using MinValueURetType = std::conditional_t<
    (std::numeric_limits<T1>::digits < std::numeric_limits<T2>::digits), T1, T2>;
#endif

/**
 * Return the smaller of two unsigned integers as the narrower type.
 * 
 * This deduces the return type to be the narrower type of `T1` and `T2`, or `T2` if they
 * are equally wide. This can be compared to @ref MinValue, where the operands and the
 * result all have the same type.
 * 
 * Below is a typical use case where this is practical:
 * 
 * ```
 * // We have a value which we want to limit to no more than fits into uint16_t.
 * uint32_t a = ...;
 * // Use MinValueU and assign the result to uint16_t (no cast needed).
 * uint16_t b = MinValueU(a, std::numeric_limits<uint16_t>::max());
 * ```
 * 
 * @tparam T1 Type of first operand. Must be an unsigned integer type.
 * @tparam T2 Type of second operand. Must be an unsigned integer type.
 * @param op1 First operand.
 * @param op2 Second operand.
 * @return The smaller operand, as the narrower type (see description).
 */
template <typename T1, typename T2>
constexpr MinValueURetType<T1, T2> MinValueU (T1 op1, T2 op2)
{
    static_assert(std::is_unsigned<T1>::value, "Only unsigned allowed");
    static_assert(std::is_unsigned<T2>::value, "Only unsigned allowed");
    
    if (op1 < op2) {
        return op1;
    } else {
        return op2;
    }
}

/** @} */

}

#endif
