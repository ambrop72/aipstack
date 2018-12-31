/*
 * Copyright (c) 2018 Ambroz Bizjak
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

#ifndef AIPSTACK_INTERVAL_UTILS_H
#define AIPSTACK_INTERVAL_UTILS_H

#include <type_traits>

namespace AIpStack {

/**
 * @ingroup misc
 * @defgroup interval-utils Interval Utilities
 * @brief Utilities related to intervals.
 * 
 * @{
 */

/**
 * Check if a value is in a half-open interval given by a non-inclusive start
 * point and the interval length, under modular arithmetic.
 * 
 * This determines whether `x` is in the half-open interval
 * (`start`, `start`+`length`] under modular arithmetic as per the `IntType`
 * unsigned integer type.
 * 
 * Thanks to Simon Stienen for this most efficient formula.
 * 
 * @tparam IntType Type of operands. Must be an unsigned integer type.
 * @param start Start of the interval, non-inclusive.
 * @param length Length of the interval.
 * @param x Value for which to check whether it is in the interval.
 * @return Whether `x` is in the interval (`start`, `start`+`length`].
 */
template<typename IntType>
bool InOpenClosedIntervalStartLen (IntType start, IntType length, IntType x)
{
    static_assert(std::is_unsigned_v<IntType>, "Must be unsigned");
    
    return IntType(x + ~start) < length;
}

/** @} */

}

#endif
