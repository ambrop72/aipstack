/*
 * Copyright (c) 2014 Ambroz Bizjak
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

#ifndef AIPSTACK_TYPE_SEQUENCE_H
#define AIPSTACK_TYPE_SEQUENCE_H

#include <aipstack/meta/BasicMetaUtils.h>

namespace AIpStack {

/**
 * @addtogroup meta
 * @{
 */

template <typename... Types>
struct TypeSequence {};

#ifndef IN_DOXYGEN

template <typename, typename>
struct TypeSequenceMakeIntConcatHelper;

template <typename... Ints1, typename... Ints2>
struct TypeSequenceMakeIntConcatHelper<TypeSequence<Ints1...>, TypeSequence<Ints2...>> {
    using Result = TypeSequence<Ints1..., Ints2...>;
};

template <int S, int N>
struct TypeSequenceMakeIntHelper {
    using Result = typename TypeSequenceMakeIntConcatHelper<
        typename TypeSequenceMakeIntHelper<S, (N / 2)>::Result,
        typename TypeSequenceMakeIntHelper<S + (N / 2), N - (N / 2)>::Result
    >::Result;
};

template <int S>
struct TypeSequenceMakeIntHelper<S, 0> {
    using Result = TypeSequence<>;
};

template <int S>
struct TypeSequenceMakeIntHelper<S, 1> {
    using Result = TypeSequence<WrapInt<S>>;
};

#endif

template <int N>
using TypeSequenceMakeInt =
#ifdef IN_DOXYGEN
implementation_hidden;
#else
typename TypeSequenceMakeIntHelper<0, N>::Result;
#endif

/** @} */

}

#endif
