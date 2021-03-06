/*
 * Copyright (c) 2013 Ambroz Bizjak
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

#ifndef AIPSTACK_BITS_IN_INT_H
#define AIPSTACK_BITS_IN_INT_H

#include <cstdint>

namespace AIpStack {

/**
 * @addtogroup meta
 * @{
 */

#ifndef IN_DOXYGEN

namespace BitsInIntPrivate {
    template<int Base, std::uintmax_t N>
    struct DigitsInInt {
        inline static constexpr int Value = 1 + DigitsInInt<Base, N / Base>::Value;
    };

    template<int Base>
    struct DigitsInInt<Base, 0> {
        inline static constexpr int Value = 0;
    };
}

#endif

template<std::uintmax_t N>
inline constexpr int BitsInInt =
    #ifdef IN_DOXYGEN
    implementation_hidden;
    #else
    BitsInIntPrivate::DigitsInInt<2, N>::Value;
    #endif

template<std::uintmax_t N>
inline constexpr int HexDigitsInInt =
    #ifdef IN_DOXYGEN
    implementation_hidden;
    #else
    BitsInIntPrivate::DigitsInInt<16, N>::Value;
    #endif

/** @} */

}

#endif
