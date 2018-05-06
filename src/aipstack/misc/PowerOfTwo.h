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

#ifndef AIPSTACK_POWER_OF_TWO_H
#define AIPSTACK_POWER_OF_TWO_H

namespace AIpStack {

/**
 * @addtogroup misc
 * @{
 */

/**
 * Calculate a power of two, calculated using repeated multiplication by 2.
 * 
 * This calculates a power of two recursively using the following expression:
 * `(e <= 0) ? 1 : 2 * PowerOfTwo<T>(e - 1)`. There is no consideration for overflow.
 * 
 * @tparam T Result type (integer or floating-point type).
 * @param e Exponent (negative values are treated as 0).
 * @return 2 raised to the power `max(0, e)`.
 */
template <typename T>
constexpr T PowerOfTwo (int e)
{
    return (e <= 0) ? 1 : 2 * PowerOfTwo<T>(e - 1);
}

/** @} */

}

#endif

