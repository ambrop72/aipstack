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

#ifndef AIPSTACK_BINARY_TOOLS_H
#define AIPSTACK_BINARY_TOOLS_H

#include <cstdint>

#include <type_traits>
#include <limits>

#include <aipstack/misc/Hints.h>

namespace AIpStack {

/**
 * @ingroup misc
 * @defgroup binary-tools Binary Encoding/Decoding
 * @brief Binary encoding and decoding of integers
 * 
 * The @ref ReadBinaryInt function encodes an integer to bytes and the @ref WriteBinaryInt
 * function decodes bytes to an integer. Either little-endian (@ref BinaryLittleEndian) or
 * big-endian (@ref BinaryBigEndian) byte-order can be used.
 * 
 * The supported types are all binary integer types (signed and unsigned) which are
 * 8, 16, 32 or 64-bits wide.
 * 
 * @{
 */

#ifndef IN_DOXYGEN
namespace BinaryToolsPrivate {
    template<typename T, bool IsIntegral>
    struct IntegerSupported {
        inline static constexpr bool Value = std::numeric_limits<T>::radix == 2;
    };

    template<typename T>
    struct IntegerSupported<T, false> {
        inline static constexpr bool Value = false;
    };
}
#endif

/**
 * Determine whether a type is supported for binary encoding/decoding by @ref
 * ReadBinaryInt and @ref WriteBinaryInt.
 * 
 * @tparam T Type to check.
 * @return True if the type is supported, false if not.
 */
template<typename T>
constexpr bool BinaryReadWriteSupportsType () {
    return BinaryToolsPrivate::IntegerSupported<T, std::is_integral<T>::value>::Value;
}

#ifndef IN_DOXYGEN

namespace BinaryToolsPrivate {
    
    template<int Bits>
    struct RepresentativeImpl;

    #define AIPSTACK_DEFINE_REPRESENTATIVE(bits, repr_type) \
    template<> \
    struct RepresentativeImpl<bits> { \
        using Type = repr_type; \
    };

    AIPSTACK_DEFINE_REPRESENTATIVE(8,  std::uint8_t)
    AIPSTACK_DEFINE_REPRESENTATIVE(16, std::uint16_t)
    AIPSTACK_DEFINE_REPRESENTATIVE(32, std::uint32_t)
    AIPSTACK_DEFINE_REPRESENTATIVE(64, std::uint64_t)

    #undef AIPSTACK_DEFINE_REPRESENTATIVE
    
    template<typename T>
    struct RepresentativeCheck {
        static_assert(BinaryReadWriteSupportsType<T>());
        static_assert(std::is_unsigned<T>::value);
        
        using Type = typename RepresentativeImpl<std::numeric_limits<T>::digits>::Type;
    };
    
    template<typename T>
    using Representative = typename RepresentativeCheck<T>::Type;
    
    template<typename T, bool BigEndian>
    struct ReadUnsigned {
        inline static constexpr int Bits = std::numeric_limits<T>::digits;
        static_assert(Bits % 8 == 0);
        inline static constexpr int Bytes = Bits / 8;
        
        AIPSTACK_ALWAYS_INLINE AIPSTACK_UNROLL_LOOPS
        static T readInt (char const *src)
        {
            using UChar = unsigned char;

            T val = 0;
            for (int i = 0; i < Bytes; i++) {
                int j = BigEndian ? (Bytes - 1 - i) : i;
                val |= T(UChar(src[i]) & 0xFF) << (8 * j);
            }
            return val;
        }
    };
    
    template<typename T, bool BigEndian>
    struct WriteUnsigned {
        inline static constexpr int Bits = std::numeric_limits<T>::digits;
        static_assert(Bits % 8 == 0);
        inline static constexpr int Bytes = Bits / 8;
        
        AIPSTACK_ALWAYS_INLINE AIPSTACK_UNROLL_LOOPS
        static void writeInt (T value, char *dst)
        {
            for (int i = 0; i < Bytes; i++) {
                int j = BigEndian ? (Bytes - 1 - i) : i;
                reinterpret_cast<unsigned char *>(dst)[i] = (value >> (8 * j)) & 0xFF;
            }
        }
    };
    
#if defined(__GNUC__) && defined(__BYTE_ORDER__) && \
    defined(__ARM_ARCH) && __ARM_ARCH >= 7 && \
    defined(__ARM_FEATURE_UNALIGNED) && __ARM_FEATURE_UNALIGNED
    
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define AIPSTACK_BINARYTOOLS_BIG_ENDIAN 0
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define AIPSTACK_BINARYTOOLS_BIG_ENDIAN 1
#else
#error "Unknown endian"
#endif
    
    /*
     * These implementations are for architectures which support
     * unaligned memory access, with the intention that the memcpy
     * is compiled to a single load/store instruction.
     * 
     * The code should work generally with GCC however it is not enabled
     * by default since it may result in much worse code than default
     * implementations above. For example on ARM cortex-m3 with forced
     * -mno-unaligned-access, actual memcpy calls have been seen.
     * So, specific configurations should be added in the test above
     * only after it is confirmed the result is good.
     */
    
    template<bool BigEndian>
    struct ReadUnsigned<std::uint32_t, BigEndian> {
        AIPSTACK_ALWAYS_INLINE
        static std::uint32_t readInt (char const *src)
        {
            std::uint32_t w;
            __builtin_memcpy(&w, src, sizeof(w));
            return BigEndian != AIPSTACK_BINARYTOOLS_BIG_ENDIAN ? __builtin_bswap32(w) : w;
        }
    };
    
    template<bool BigEndian>
    struct WriteUnsigned<std::uint32_t, BigEndian> {
        AIPSTACK_ALWAYS_INLINE
        static void writeInt (std::uint32_t value, char *dst)
        {
            std::uint32_t w = BigEndian != AIPSTACK_BINARYTOOLS_BIG_ENDIAN ? __builtin_bswap32(value) : value;
            __builtin_memcpy(dst, &w, sizeof(w));
        }
    };
    
    template<bool BigEndian>
    struct ReadUnsigned<std::uint16_t, BigEndian> {
        AIPSTACK_ALWAYS_INLINE
        static std::uint16_t readInt (char const *src)
        {
            std::uint16_t w;
            __builtin_memcpy(&w, src, sizeof(w));
            return BigEndian != AIPSTACK_BINARYTOOLS_BIG_ENDIAN ? __builtin_bswap16(w) : w;
        }
    };
    
    template<bool BigEndian>
    struct WriteUnsigned<std::uint16_t, BigEndian> {
        AIPSTACK_ALWAYS_INLINE
        static void writeInt (std::uint16_t value, char *dst)
        {
            std::uint16_t w = BigEndian != AIPSTACK_BINARYTOOLS_BIG_ENDIAN ? __builtin_bswap16(value) : value;
            __builtin_memcpy(dst, &w, sizeof(w));
        }
    };
    
#endif
    
    template<bool IsSigned>
    struct SignHelper {
        template<typename T, bool BigEndian>
        inline static T read_it (char const *src)
        {
            static_assert(std::is_unsigned<T>::value);
            
            return ReadUnsigned<Representative<T>, BigEndian>::readInt(src);
        }
        
        template<typename T, bool BigEndian>
        inline static void write_it (T value, char *dst)
        {
            static_assert(std::is_unsigned<T>::value);
            
            return WriteUnsigned<Representative<T>, BigEndian>::writeInt(value, dst);
        }
    };
    
    template<>
    struct SignHelper<true> {
        template<typename T, bool BigEndian>
        inline static T read_it (char const *src)
        {
            static_assert(std::is_signed<T>::value);
            using UT = std::make_unsigned_t<T>;
            
            UT uval = SignHelper<false>::template read_it<UT, BigEndian>(src);
            return reinterpret_cast<T const &>(uval);
        }
        
        template<typename T, bool BigEndian>
        inline static void write_it (T value, char *dst)
        {
            static_assert(std::is_signed<T>::value);
            using UT = std::make_unsigned_t<T>;
            
            UT uval = value;
            return SignHelper<false>::template write_it<UT, BigEndian>(uval, dst);
        }
    };
}

template<bool BigEndian_>
struct BinaryEndian {
    inline static constexpr bool BigEndian = BigEndian_;
};

#endif

/**
 * Little-endian byte order (used as a type only).
 */
using BinaryLittleEndian = BinaryEndian<false>;

/**
 * Little-endian byte order (used as a type only).
 */
using BinaryBigEndian = BinaryEndian<true>;

/**
 * Decode an integer from a binary (byte) representation.
 * 
 * @tparam T Integer type (see the @ref binary-tools module description for supported
 *         types).
 * @tparam Endian Byte order (@ref BinaryLittleEndian or @ref BinaryBigEndian).
 * @param src Pointer to encoded data; `sizeof(T)` bytes will be read from here.
 * @return Decoded integer value.
 */
template<typename T, typename Endian>
inline T ReadBinaryInt (char const *src)
{
    static_assert(BinaryReadWriteSupportsType<T>());
    
    return BinaryToolsPrivate::SignHelper<std::is_signed<T>::value>::
        template read_it<T, Endian::BigEndian>(src);
}

/**
 * Encode an integer to a binary (byte) representation.
 * 
 * @tparam T Integer type (see the @ref binary-tools module description for supported
 *         types).
 * @tparam Endian Byte order (@ref BinaryLittleEndian or @ref BinaryBigEndian).
 * @param value Integer value to encode.
 * @param dst Pointer where to write the encoded data; `sizeof(T)` bytes will be written
 *        here.
 */
template<typename T, typename Endian>
inline void WriteBinaryInt (T value, char *dst)
{
    static_assert(BinaryReadWriteSupportsType<T>());
    
    return BinaryToolsPrivate::SignHelper<std::is_signed<T>::value>::
        template write_it<T, Endian::BigEndian>(value, dst);
}

/** @} */

}

#endif
