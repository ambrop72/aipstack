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

#ifndef AIPSTACK_INT_FORMAT_H
#define AIPSTACK_INT_FORMAT_H

#include <stddef.h>

#include <type_traits>
#include <algorithm>

#include <aipstack/misc/MinMax.h>
#include <aipstack/misc/Hints.h>
#include <aipstack/misc/Assert.h>
#include <aipstack/infra/MemRef.h>

namespace AIpStack {

/**
 * @ingroup misc
 * @defgroup integer-format Integer formatting
 * @brief Utilities for formatting and parsing integers
 * 
 * @{
 */

/**
 * Determine if a type is an integer type excluding bool.
 * 
 * @tparam T Type to check.
 */
template <typename T>
constexpr bool IsInteger = std::is_integral<T>::value && !std::is_same<T, bool>::value;

#ifndef IN_DOXYGEN
namespace Private {
    template <typename T>
    constexpr size_t IntegerFormatLenUnsigned (T value)
    {
        static_assert(std::is_unsigned<T>::value, "");

        size_t len = 0;
        do {
            value /= 10;
            len++;
        } while (value > 0);

        return len;
    }

    template <typename T>
    constexpr size_t MaxIntegerFormatLenBase ()
    {
        if (std::is_signed<T>::value) {
            using UT = std::make_unsigned_t<T>;
            return 1 + IntegerFormatLenUnsigned<UT>(UT(-UT(TypeMin<T>())));
        } else {
            return IntegerFormatLenUnsigned<T>(TypeMax<T>());
        }
    }
}
#endif

/**
 * Maximum number of characters that @ref FormatInteger may write for the given type
 * including the null terminator.
 * 
 * @tparam T Integer type (excluding bool), see @ref IsInteger.
 */
template <typename T, typename = std::enable_if_t<IsInteger<T>>>
constexpr size_t MaxIntegerFormatLen = Private::MaxIntegerFormatLenBase<T>() + 1;

/**
 * Format an integer to decimal representation.
 * 
 * This generates the decimal representation of the integer without any redundant leading
 * zeros and with a leading minus sign in case of negative values.
 * 
 * @tparam T Integer type (excluding bool), see @ref IsInteger.
 * @param value Integer to be formatted.
 * @param out_str Pointer to where the result will be written including a null terminator.
 *        It must not be null and there must be at least @ref MaxIntegerFormatLen<T> bytes
 *        available.
 * @return Pointer to one past the last non-null character written (and pointer to the
 *         written null terminator).
 */
template <typename T, typename = std::enable_if_t<IsInteger<T>>>
AIPSTACK_OPTIMIZE_SIZE
char * FormatInteger (T value, char *out_str)
{
    using UT = std::make_unsigned_t<T>;

    bool isNegative = (value < 0);
    UT uval = isNegative ? UT(-UT(value)) : value;

    char *pos = out_str;

    do {
        *pos++ = '0' + (uval % 10);
        uval /= 10;
    } while (uval > 0);

    if (isNegative) {
        *pos++ = '-';
    }

    std::reverse(out_str, pos);

    *pos = '\0';

    return pos;
}

/**
 * Parse an integer in decimal representation.
 * 
 * This accepts any decimal representation consisting of an optional minus sign (only for
 * unsigned `T`) followed by one or more decimal digits where the encoded value is
 * representable in the integer type `T`. Other inputs are rejected.
 * 
 * @tparam T Integer type (excluding bool), see @ref IsInteger.
 * @param str Input data to parse (`str.ptr` must not be null).
 * @param out_value On success, is set to the parsed integer value (not changed on failure).
 * @return True on success, false on failure.
 */
template <typename T, typename = std::enable_if_t<IsInteger<T>>>
AIPSTACK_OPTIMIZE_SIZE
bool ParseInteger (MemRef str, T &out_value)
{
    AIPSTACK_ASSERT(str.ptr != nullptr)

    using UT = std::make_unsigned_t<T>;

    char const *ptr = str.ptr;
    char const *end = str.ptr + str.len;

    if (ptr == end) {
        return false;
    }

    bool isNegative = false;

    if (std::is_signed<T>::value && *ptr == '-') {
        ptr++;
        isNegative = true;

        if (ptr == end) {
            return false;
        }
    }

    UT ulimit = isNegative ? UT(-UT(TypeMin<T>())) : TypeMax<T>();
    UT ulimit_10 = ulimit / 10;

    UT uvalue = 0;

    do {
        char ch = *ptr++;

        unsigned char digit_val = (unsigned char)ch - (unsigned char)'0';
        if (digit_val > 9) {
            return false;
        }

        if (uvalue > ulimit_10) {
            return false;
        }
        uvalue *= 10;

        if (digit_val > ulimit - uvalue) {
            return false;
        }
        uvalue += digit_val;
    } while (ptr != end);

    T value;
    if (isNegative) {
        value = (uvalue == 0) ? 0 : (-T(uvalue - 1) - 1);
    } else {
        value = T(uvalue);
    }

    out_value = value;
    return true;
}

/** @} */

}

#endif
