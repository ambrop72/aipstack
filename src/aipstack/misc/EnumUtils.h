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

#ifndef AIPSTACK_ENUM_UTILS_H
#define AIPSTACK_ENUM_UTILS_H

#include <type_traits>

#include <aipstack/meta/BasicMetaUtils.h>

namespace AIpStack {

/**
 * @ingroup misc
 * @defgroup enum-utils Enum Utilities
 * @brief Utilities for enum types.
 * 
 * @{
 */

/**
 * An empty class which represents a zero value for any enum type.
 * 
 * This class provides a template conversion operator to any enum type
 * which returns an enum with zero value. Additionally, `==` and `!=` operators
 * are available for comparing an enum type and @ref Enum0Type (in any order).
 * 
 * This class should be used via the @ref Enum0 constant which is an easily
 * accessible value of @ref Enum0Type type.
 */
class Enum0Type {
public:
    /**
     * Conversion operator to a zero value of any enum type.
     * 
     * @tparam EnumType Target enum type (restricted to enum types using SFINAE).
     * @return `EnumType(0)`
     */
    template<typename EnumType,
        typename = std::enable_if_t<std::is_enum<EnumType>::value>>
    inline constexpr operator EnumType () const {
        return EnumType(0);
    }
};

/**
 * Check if an enum value is equal to zero.
 * 
 * @tparam EnumType Type of enum operand (restricted to enum types using SFINAE).
 * @param e Enum value.
 * @return `e == EnumType(0)`
 */
template<typename EnumType,
    typename = std::enable_if_t<std::is_enum<EnumType>::value>>
inline constexpr bool operator== (EnumType e, Enum0Type) {
    return e == EnumType(0);
}

/**
 * Check if an enum value is equal to zero.
 * 
 * @tparam EnumType Type of enum operand (restricted to enum types using SFINAE).
 * @param e Enum value.
 * @return `e == EnumType(0)`
 */
template<typename EnumType,
    typename = std::enable_if_t<std::is_enum<EnumType>::value>>
inline constexpr bool operator== (Enum0Type, EnumType e) {
    return e == EnumType(0);
}

/**
 * Check if an enum value is different from zero.
 * 
 * @tparam EnumType Type of enum operand (restricted to enum types using SFINAE).
 * @param e Enum value.
 * @return `e != EnumType(0)`
 */
template<typename EnumType,
    typename = std::enable_if_t<std::is_enum<EnumType>::value>>
inline constexpr bool operator!= (EnumType e, Enum0Type) {
    return e != EnumType(0);
}

/**
 * Check if an enum value is different from zero.
 * 
 * @tparam EnumType Type of enum operand (restricted to enum types using SFINAE).
 * @param e Enum value.
 * @return `e != EnumType(0)`
 */
template<typename EnumType,
    typename = std::enable_if_t<std::is_enum<EnumType>::value>>
inline constexpr bool operator!= (Enum0Type, EnumType e) {
    return e != EnumType(0);
}

/**
 * An @ref Enum0Type value for convenience.
 */
inline constexpr Enum0Type Enum0 = Enum0Type();

/**
 * Convert an enum to its underlying type.
 * 
 * @tparam EnumType Enum type.
 * @param e Enum value.
 * @return Value converted to underlying type.
 */
template<typename EnumType>
inline constexpr std::underlying_type_t<EnumType> AsUnderlying (EnumType e)
{
    static_assert(std::is_enum<EnumType>::value, "EnumType must be an enum type");
    return std::underlying_type_t<EnumType>(e);
}

#ifndef IN_DOXYGEN

namespace EnumUtilsPrivate {
    template<bool IsEnum, typename Type, typename BaseType>
    struct EnumWithBaseTypeHelper {
        inline static constexpr bool IsEnumWithBaseType = false;
    };
    
    template<typename Type, typename BaseType>
    struct EnumWithBaseTypeHelper<true, Type, BaseType> {
        inline static constexpr bool IsEnumWithBaseType =
            std::is_same<std::underlying_type_t<Type>, BaseType>::value;
    };
    
    template<bool IsEnum, typename Type>
    struct GetSameOrBaseTypeHelper {
        using ResultType = Type;
    };
    
    template<typename Type>
    struct GetSameOrBaseTypeHelper<true, Type> {
        using ResultType = std::underlying_type_t<Type>;
    };
}

#endif

/**
 * Check if a type is an enum type with the specified underlying type.
 * 
 * The result is @ref WrapBool<true> if `Type` is an enum type with underlying type
 * `BaseType`, otherwise the result is @ref WrapBool<false>.
 * 
 * @tparam Type Type to check.
 * @tparam BaseType Underlying type to check for.
 */
template<typename Type, typename BaseType>
using IsEnumWithBaseType = WrapBool<
    #ifdef IN_DOXYGEN
    implementation_hidden
    #else
    EnumUtilsPrivate::EnumWithBaseTypeHelper<std::is_enum<Type>::value, Type, BaseType>::IsEnumWithBaseType
    #endif
>;

/**
 * Check if a type is the specified type or an enum type with that underlying type.
 * 
 * The result is @ref WrapBool<true> if `Type` is `BaseType` or an enum type with underlying
 * type `BaseType`, otherwise the result is @ref WrapBool<false>.
 * 
 * @tparam Type Type to check.
 * @tparam BaseType Type or underlying type to check for.
 */
template<typename Type, typename BaseType>
using IsSameOrEnumWithBaseType = WrapBool<
    #ifdef IN_DOXYGEN
    implementation_hidden
    #else
    std::is_same<Type, BaseType>::value || IsEnumWithBaseType<Type, BaseType>::Value
    #endif
>;

/**
 * Get the underlying type of an enum type or if not an enum the type itself.
 * 
 * If `Type` is an enum type the result is the underlying type, otherwise the result is
 * `Type`.
 * 
 * @tparam Type Type for which to get the same or underlying type.
 */
template<typename Type>
using GetSameOrEnumBaseType = typename EnumUtilsPrivate::GetSameOrBaseTypeHelper<std::is_enum<Type>::value, Type>::ResultType;

/** @} */

}

#endif
