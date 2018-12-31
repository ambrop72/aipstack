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

#ifndef AIPSTACK_IP_ADDR_H
#define AIPSTACK_IP_ADDR_H

#include <cstdint>
#include <cstddef>

#include <aipstack/misc/Assert.h>
#include <aipstack/misc/MinMax.h>
#include <aipstack/infra/Struct.h>

namespace AIpStack {

/**
 * @addtogroup ip-stack
 * @{
 */

/**
 * Represents an IPv4 address.
 * 
 * An @ref Ip4Addr object is defined by its value which is a 32-bit unsigned
 * integer representing an IPv4 address in "native byte order". The @ref ValueInt
 * type alias defines the integer type used.
 * 
 * An @ref Ip4Addr object with a specific value can be constructed using the
 * constructor @ref Ip4Addr(ValueInt), and the value of an object can be obtained
 * using @ref value().
 * 
 * Functions for representing and parsing IPv4 addresses using dot-decimal
 * notation are available in the @ref ip-addr-format module.
 * 
 * @ref Ip4Addr may be used as a field type in header definitions based on the
 * @ref struct system; `get` and `set` operations on such a field use the @ref
 * Ip4Addr type directly.
 */
class Ip4Addr {
public:
    /**
     * Size of an IPv4 address in bytes.
     */
    inline static constexpr std::size_t Size = 4;
    
    /**
     * Size of an IPv4 address in bits.
     */
    inline static constexpr std::size_t Bits = 32;

    /**
     * Integer type used to represent the value of an IPv4 address.
     */
    using ValueInt = std::uint32_t;

private:
    ValueInt m_value;

public:
    /**
     * Default constructor, constructs an address with zero value.
     */
    inline constexpr Ip4Addr () :
        m_value(0)
    {}

    /**
     * Constructor from a specific value.
     * 
     * @param value Value of the address.
     */
    inline explicit constexpr Ip4Addr (ValueInt value) :
        m_value(value)
    {}

    /**
     * Constructor from four octet values.
     * 
     * @param n1 First octet (most significant).
     * @param n2 Second octet.
     * @param n3 Third octet.
     * @param n4 Fourth octet (least significant).
     */
    inline explicit constexpr Ip4Addr (
        std::uint8_t n1, std::uint8_t n2, std::uint8_t n3, std::uint8_t n4)
    :
        Ip4Addr(ValueInt(
            (ValueInt(n1) << 24) |
            (ValueInt(n2) << 16) |
            (ValueInt(n3) << 8) |
            (ValueInt(n4) << 0)))
    {}

    /**
     * Get the value of the address.
     * 
     * @return The value of the address.
     */
    inline constexpr ValueInt value () const {
        return m_value;
    }

    /**
     * Equal-to operator.
     * 
     * @param other The other address to compare.
     * @return `value() == other.value()`
     */
    inline constexpr bool operator==(Ip4Addr other) const {
        return value() == other.value();
    }

    /**
     * Not-equal-to operator.
     * 
     * @param other The other address to compare.
     * @return `value() != other.value()`
     */
    inline constexpr bool operator!=(Ip4Addr other) const {
        return value() != other.value();
    }

    /**
     * Less-than operator.
     * 
     * @param other The other address to compare.
     * @return `value() < other.value()`
     */
    inline constexpr bool operator<(Ip4Addr other) const {
        return value() < other.value();
    }

    /**
     * Less-than-or-equal operator.
     * 
     * @param other The other address to compare.
     * @return `value() <= other.value()`
     */
    inline constexpr bool operator<=(Ip4Addr other) const {
        return value() <= other.value();
    }

    /**
     * Greater-than operator.
     * 
     * @param other The other address to compare.
     * @return `value() > other.value()`
     */
    inline constexpr bool operator>(Ip4Addr other) const {
        return value() > other.value();
    }

    /**
     * Greater-than-or-equal operator.
     * 
     * @param other The other address to compare.
     * @return `value() >= other.value()`
     */
    inline constexpr bool operator>=(Ip4Addr other) const {
        return value() >= other.value();
    }

    /**
     * Binary AND operator.
     * 
     * @param other The other address.
     * @return Address with value `value() & other.value()`.
     */
    inline constexpr Ip4Addr operator& (Ip4Addr other) const {
        return Ip4Addr(ValueInt(value() & other.value()));
    }
    
    /**
     * Binary OR operator.
     * 
     * @param other The other address.
     * @return Address with value `value() | other.value()`.
     */
    inline constexpr Ip4Addr operator| (Ip4Addr other) const {
        return Ip4Addr(ValueInt(value() | other.value()));
    }
    
    /**
     * Binary complement operator.
     * 
     * @return Address with value `~value()`.
     */
    inline constexpr Ip4Addr operator~ () const {
        return Ip4Addr(ValueInt(~value()));
    }
    
    /**
     * Return an address with zero value.
     * 
     * @return Address with zero value.
     */
    inline static constexpr Ip4Addr ZeroAddr () {
        return Ip4Addr(ValueInt(0));
    }
    
    /**
     * Return an address with the maximum value (all bits one).
     * 
     * @return Address with the maximum value.
     */
    inline static constexpr Ip4Addr AllOnesAddr () {
        return Ip4Addr(TypeMax<ValueInt>());
    }
    
    /**
     * Return an address with the given number of most-significant bits one.
     * 
     * This can be used to create a network mask for some prefix length.
     * 
     * @param prefix_bits Number of most-significant bits which should be one.
     *        Must be less than or equal to @ref Bits (32).
     * @return Address with that many most-significant bits one.
     */
    static Ip4Addr PrefixMask (std::size_t prefix_bits)
    {
        AIPSTACK_ASSERT(prefix_bits <= Bits)
        
        return
            (prefix_bits == 0) ? ZeroAddr() :
            (prefix_bits >= Bits) ? AllOnesAddr() :
            Ip4Addr(ValueInt(~((ValueInt(1) << (Bits - prefix_bits)) - 1)));
    }
    
    /**
     * Return an address with the given number of most-significant bits one,
     * passed as a template parameter.
     * 
     * This is just like @ref PrefixMask(std::size_t) except that the number of
     * bits is specified as a template parameter.
     * 
     * @tparam PrefixBits Number of most-significant bits which should be one.
     *         Must be less than or equal to @ref Bits (32).
     * @return Address with that many most-significant bits one.
     */
    template <std::size_t PrefixBits>
    static constexpr Ip4Addr PrefixMask ()
    {
        static_assert(PrefixBits <= Bits);
        
        return
            (PrefixBits == 0) ? ZeroAddr() :
            (PrefixBits >= Bits) ? AllOnesAddr() :
            Ip4Addr(ValueInt(~((ValueInt(1) << (Bits - PrefixBits)) - 1)));
    }
    
    /**
     * Create an address by selecting bits from two addresses based on a mask.
     * 
     * Bits which are one in the mask will be taken from `first` and bits which
     * are zero in the mask will be taken from `second`.
     * 
     * @param mask Mask defining which bits to take from which address.
     * @param first First address to join.
     * @param second Second address to join.
     * @return `(first & mask) | (second & ~mask)`
     */
    inline static constexpr Ip4Addr Join (Ip4Addr mask, Ip4Addr first, Ip4Addr second) {
        return (first & mask) | (second & ~mask);
    }
    
    /**
     * Check whether the value of the address is zero.
     * 
     * @return Whether the value is zero.
     */
    inline constexpr bool isZero () const {
        return value() == 0;
    }
    
    /**
     * Check whether the value of the address is the maximum value (all bits one).
     * 
     * @return Whether the value is the maximum value.
     */
    inline constexpr bool isAllOnes () const {
        return value() == TypeMax<ValueInt>();
    }
    
    /**
     * Count the number of leading one bits.
     * 
     * @return Number of subsequent one bits when counting from the most
     *         significant bit, ranging from zero to @ref Bits (32).
     */
    constexpr std::size_t countLeadingOnes () const
    {
        ValueInt val = value();
        std::size_t leading_ones = 0;
        for (std::size_t bit = Bits - 1; bit != std::size_t(-1); bit--) {
            if ((val & (ValueInt(1) << bit)) == 0) {
                break;
            }
            leading_ones++;
        }
        return leading_ones;
    }

    /**
     * Get the octet at the given index of the address, specified as a template
     * parameter.
     * 
     * @tparam ByteIndex Index of the octet where zero refers to the most
     *         significant octet. Must be less than @ref Size (4).
     * @return Value of octet at the given index.
     */
    template <std::size_t ByteIndex>
    inline constexpr std::uint8_t getByte () const {
        static_assert(ByteIndex < Size);

        return (value() >> (8 * (Size - 1 - ByteIndex))) & 0xFF;
    }
    
    /**
     * Check whether the address is a multicast address.
     * 
     * @return `(*this & Ip4Addr(0xF0, 0, 0, 0)) == Ip4Addr(0xE0, 0, 0, 0)`
     */
    inline constexpr bool isMulticast () const {
        return (*this & Ip4Addr(0xF0, 0, 0, 0)) == Ip4Addr(0xE0, 0, 0, 0);
    }
    
    /**
     * Check whether the address is a multicast address or has the maximal
     * value (all bits one).
     * 
     * @return `isAllOnes() || isMulticast()`
     */
    inline constexpr bool isAllOnesOrMulticast () const {
        return isAllOnes() || isMulticast();
    }

    /**
     * Decode an address from a byte representation using big-endian byte order.
     *
     * @param src Memory location to read from; @ref Size (4) bytes must be available.
     * @return Decoded address.
     */
    inline static Ip4Addr readBinary (char const *src) {
        return Ip4Addr(ReadSingleField<ValueInt>(src));
    }

    /**
     * Encode the address to a byte representation using big-endian byte order.
     * 
     * @param dst Memory location to write to; @ref Size (4) bytes will be written.
     */
    inline void writeBinary (char *dst) const {
        WriteSingleField<ValueInt>(dst, value());
    }
};

#ifndef IN_DOXYGEN
template <>
struct StructTypeHandler<Ip4Addr, void> {
    using Handler = StructConventionalTypeHandler<Ip4Addr>;
};
#endif

/**
 * A pair of local and remote IPv4 addresses.
 */
struct Ip4AddrPair {
    Ip4Addr local_addr; /**< Local address. */
    Ip4Addr remote_addr; /**< Remote address. */
};

/**
 * Unsigned 16-bit integer type used for port numbers.
 */
using PortNum = std::uint16_t;

/** @} */

}

#endif
