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

#ifndef AIPSTACK_ENUM_BITFIELD_UTILS_H
#define AIPSTACK_ENUM_BITFIELD_UTILS_H

#include <type_traits>

#include <aipstack/misc/EnumUtils.h>

namespace AIpStack {

/**
 * @ingroup misc
 * @defgroup enum-bitfield Enum Bitfield Utilities
 * @brief Bitwise and other operators for enum types
 * 
 * This module provides implementations of various operators for enum types with
 * bitfield semantics; see @ref AIPSTACK_ENUM_BITFIELD.
 * @{
 */

#ifndef IN_DOXYGEN
template<typename EnumType>
class EnableEnumBitfield;
#endif

/**
 * Enables various operators for an enum type with bitfield semantics.
 * 
 * @note This macro may only be used in namespace scope within the @ref AIpStack
 * namespace. There should be no semicolon after the invocation.
 * 
 * It is suggested to invoke this macro right after the definition of the
 * enum, for example:
 * 
 * ```
 * enum class MyBitfield {
 *     Field1 = 1 << 0,
 *     Field2 = 1 << 1,
 *     Field3 = 1 << 2,
 * };
 * AIPSTACK_ENUM_BITFIELD(MyBitfield)
 * ```
 * 
 * After this macro is invoked, the following bitwise operators will be available for
 * `EnumType`, performing the corresponding operation on the underlying type:
 * `~`, `|`, `&`, `^`, `|=`, `&=`, `^=`.
 * 
 * The @ref AIpStack::Enum0 "Enum0" constant can be used to initialize bitfield
 * (actually all) enums to zero and compare to zero, for example:
 * 
 * ```
 * MyBitfield x = Enum0;
 * if (x == Enum0) { ... }
 * if (x != Enum0) { ... }
 * ```
 * 
 * The @ref EnumBitfieldUtils.h header includes @ref EnumUtils.h so the latter
 * does not need to be explicitly included in order to use @ref AIpStack::Enum0
 * "Enum0".
 * 
 * @param EnumType Enum type to define operators for.
 */
#ifdef IN_DOXYGEN
#define AIPSTACK_ENUM_BITFIELD(EnumType) implementation_hidden
#else
#define AIPSTACK_ENUM_BITFIELD(EnumType) \
static_assert(std::is_enum<EnumType>::value, "AIPSTACK_ENUM_BITFIELD: not an enum"); \
template<> \
class EnableEnumBitfield<EnumType> { \
public: \
    using Enabled = void; \
};
#endif

#ifndef IN_DOXYGEN

#define AIPSTACK_ENABLE_IF_ENUM_BITFIELD(EnumType) \
    typename = typename EnableEnumBitfield<EnumType>::Enabled

template<typename EnumType, AIPSTACK_ENABLE_IF_ENUM_BITFIELD(EnumType)>
inline constexpr EnumType operator~ (EnumType arg)
{
    return EnumType(~AsUnderlying(arg));
}

template<typename EnumType, AIPSTACK_ENABLE_IF_ENUM_BITFIELD(EnumType)>
inline constexpr EnumType operator| (EnumType arg1, EnumType arg2)
{
    return EnumType(AsUnderlying(arg1) | AsUnderlying(arg2));
}

template<typename EnumType, AIPSTACK_ENABLE_IF_ENUM_BITFIELD(EnumType)>
inline constexpr EnumType operator& (EnumType arg1, EnumType arg2)
{
    return EnumType(AsUnderlying(arg1) & AsUnderlying(arg2));
}

template<typename EnumType, AIPSTACK_ENABLE_IF_ENUM_BITFIELD(EnumType)>
inline constexpr EnumType operator^ (EnumType arg1, EnumType arg2)
{
    return EnumType(AsUnderlying(arg1) ^ AsUnderlying(arg2));
}

template<typename EnumType, AIPSTACK_ENABLE_IF_ENUM_BITFIELD(EnumType)>
inline constexpr EnumType & operator|= (EnumType &arg1, EnumType arg2)
{
    arg1 = EnumType(AsUnderlying(arg1) | AsUnderlying(arg2));
    return arg1;
}

template<typename EnumType, AIPSTACK_ENABLE_IF_ENUM_BITFIELD(EnumType)>
inline constexpr EnumType & operator&= (EnumType &arg1, EnumType arg2)
{
    arg1 = EnumType(AsUnderlying(arg1) & AsUnderlying(arg2));
    return arg1;
}

template<typename EnumType, AIPSTACK_ENABLE_IF_ENUM_BITFIELD(EnumType)>
inline constexpr EnumType & operator^= (EnumType &arg1, EnumType arg2)
{
    arg1 = EnumType(AsUnderlying(arg1) ^ AsUnderlying(arg2));
    return arg1;
}

#undef AIPSTACK_ENABLE_IF_ENUM_BITFIELD

#endif

/** @} */

}

#endif
