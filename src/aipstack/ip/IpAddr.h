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
#include <aipstack/misc/BinaryTools.h>
#include <aipstack/infra/Struct.h>

namespace AIpStack {

class Ip4Addr {
public:
    static constexpr std::size_t Size = 4;
    
    static constexpr std::size_t Bits = 32;

    using ValueInt = std::uint32_t;

private:
    ValueInt m_value;

public:
    inline constexpr Ip4Addr () :
        m_value(0)
    {}

    inline explicit constexpr Ip4Addr (ValueInt value) :
        m_value(value)
    {}

    inline explicit constexpr Ip4Addr (
        std::uint8_t n1, std::uint8_t n2, std::uint8_t n3, std::uint8_t n4)
    :
        Ip4Addr(ValueInt(
            (ValueInt(n1) << 24) |
            (ValueInt(n2) << 16) |
            (ValueInt(n3) << 8) |
            (ValueInt(n4) << 0)))
    {}

    inline constexpr ValueInt value () const {
        return m_value;
    }

    inline constexpr bool operator==(Ip4Addr other) const {
        return value() == other.value();
    }

    inline constexpr bool operator!=(Ip4Addr other) const {
        return value() != other.value();
    }

    inline constexpr bool operator<(Ip4Addr other) const {
        return value() < other.value();
    }

    inline constexpr bool operator<=(Ip4Addr other) const {
        return value() <= other.value();
    }

    inline constexpr bool operator>(Ip4Addr other) const {
        return value() > other.value();
    }

    inline constexpr bool operator>=(Ip4Addr other) const {
        return value() >= other.value();
    }

    inline constexpr Ip4Addr operator& (Ip4Addr other) const {
        return Ip4Addr(ValueInt(value() & other.value()));
    }
    
    inline constexpr Ip4Addr operator| (Ip4Addr other) const {
        return Ip4Addr(ValueInt(value() | other.value()));
    }
    
    inline constexpr Ip4Addr operator~ () const {
        return Ip4Addr(ValueInt(~value()));
    }
    
    inline static constexpr Ip4Addr ZeroAddr () {
        return Ip4Addr(ValueInt(0));
    }
    
    inline static constexpr Ip4Addr AllOnesAddr () {
        return Ip4Addr(TypeMax<ValueInt>());
    }
    
    static Ip4Addr PrefixMask (std::size_t prefix_bits)
    {
        AIPSTACK_ASSERT(prefix_bits <= Bits)
        
        return
            (prefix_bits == 0) ? ZeroAddr() :
            (prefix_bits >= Bits) ? AllOnesAddr() :
            Ip4Addr(ValueInt(~((ValueInt(1) << (Bits - prefix_bits)) - 1)));
    }
    
    template <std::size_t PrefixBits>
    static constexpr Ip4Addr PrefixMask ()
    {
        static_assert(PrefixBits <= Bits, "");
        
        return
            (PrefixBits == 0) ? ZeroAddr() :
            (PrefixBits >= Bits) ? AllOnesAddr() :
            Ip4Addr(ValueInt(~((ValueInt(1) << (Bits - PrefixBits)) - 1)));
    }
    
    inline static constexpr Ip4Addr Join (Ip4Addr mask, Ip4Addr first, Ip4Addr second) {
        return (first & mask) | (second & ~mask);
    }
    
    inline constexpr bool isZero () const {
        return value() == 0;
    }
    
    inline constexpr bool isAllOnes () const {
        return value() == TypeMax<ValueInt>();
    }
    
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

    template <std::size_t ByteIndex>
    inline constexpr std::uint8_t getByte () const {
        static_assert(ByteIndex < Size, "");

        return (value() >> (8 * (Size - 1 - ByteIndex))) & 0xFF;
    }
    
    inline constexpr bool isMulticast () const {
        return (*this & Ip4Addr(0xF0, 0, 0, 0)) == Ip4Addr(0xE0, 0, 0, 0);
    }
    
    inline constexpr bool isAllOnesOrMulticast () const {
        return isAllOnes() || isMulticast();
    }

    inline static Ip4Addr readBinary (char const *src) {
        return Ip4Addr(ReadBinaryInt<ValueInt, BinaryBigEndian>(src));
    }

    inline void writeBinary (char *dst) const {
        WriteBinaryInt<ValueInt, BinaryBigEndian>(value(), dst);
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
struct Ip4Addrs {
    Ip4Addr local_addr; /**< Local address. */
    Ip4Addr remote_addr; /**< Remote address. */
};

/**
 * Unsigned 16-bit integer type used for port numbers.
 */
using PortNum = std::uint16_t;

}

#endif
