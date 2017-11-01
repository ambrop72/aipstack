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

#ifndef AIPSTACK_ONE_OF_H
#define AIPSTACK_ONE_OF_H

#include <aipstack/misc/Hints.h>

namespace AIpStack {

/**
 * @addtogroup misc
 * @{
 */

#ifndef IN_DOXYGEN

template <typename...>
struct OneOfStruct;

template <typename OptRefType, typename... TailOptRefType>
struct OneOfStruct<OptRefType, TailOptRefType...> {
    AIPSTACK_ALWAYS_INLINE
    constexpr OneOfStruct (OptRefType const &opt_ref_arg, TailOptRefType const & ... tail_opt_ref_arg)
    : opt_ref(opt_ref_arg), tail_opt_ref(tail_opt_ref_arg...) {}
    
    template <typename SelType>
    AIPSTACK_ALWAYS_INLINE
    constexpr bool one_of (SelType const &sel) const
    {
        return sel == opt_ref || tail_opt_ref.one_of(sel);
    }
    
    OptRefType opt_ref;
    OneOfStruct<TailOptRefType...> tail_opt_ref;
};

template <>
struct OneOfStruct<> {
    AIPSTACK_ALWAYS_INLINE
    constexpr OneOfStruct () {}
    
    template <typename SelType>
    AIPSTACK_ALWAYS_INLINE
    constexpr bool one_of (SelType const &sel) const
    {
        return false;
    }
};

template <typename SelType, typename... OptRefType>
AIPSTACK_ALWAYS_INLINE
bool operator== (SelType const &sel, OneOfStruct<OptRefType...> opt_struct)
{
    return opt_struct.one_of(sel);
}

template <typename SelType, typename... OptRefType>
AIPSTACK_ALWAYS_INLINE
bool operator!= (SelType const &sel, OneOfStruct<OptRefType...> opt_struct)
{
    return !opt_struct.one_of(sel);
}

#endif

/**
 * Use to check if a value is equal to or not equal to any of the options.
 * 
 * This should be used according to the following recipe:
 * 
 * ```
 * X == OneOf(O1, ..., On)
 * X != OneOf(O1, ..., On)
 * ```
 * 
 * When one of the these comparison expressions is used, the operand `X` is compared to
 * the options in order using `X == Oi`. If any option matches, the result is true for `==`
 * or false for `!=` and the remaining options are not checked; if no option matches the
 * result is false or true respectively.
 * 
 * @note
 * The option values are copied and stored in the returned structure. Since the intention is
 * to be used with integers/enums and inlining is forced, it should regardless have no
 * special overhead.
 * 
 * @tparam OptType Types of options. Each type must be copy-constructible.
 * @param opt Option values.
 * @return A value containing all the option values and for which `==` and `!=` operators
 *         are defined to support the expressions described.
 */
template <typename... OptType>
AIPSTACK_ALWAYS_INLINE
OneOfStruct<OptType...> OneOf (OptType ... opt)
{
    return OneOfStruct<OptType...>(opt...);
}

/** @} */

}

#endif
