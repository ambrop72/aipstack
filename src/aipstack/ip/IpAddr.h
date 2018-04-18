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

#include <stdint.h>
#include <stddef.h>

#include <type_traits>

#include <aipstack/misc/Assert.h>
#include <aipstack/infra/Struct.h>

namespace AIpStack {

template <typename AddrType, typename ElemType, size_t Length>
class IpGenericAddr : public StructIntArray<ElemType, Length>
{
    static_assert(std::is_unsigned<ElemType>::value, "");
    
public:
    static size_t const Bits      = 8 * IpGenericAddr::Size;
    static size_t const ElemBits  = 8 * IpGenericAddr::ElemSize;
    
public:
    static inline constexpr AddrType ZeroAddr ()
    {
        return AddrType{};
    }
    
    static inline constexpr AddrType AllOnesAddr ()
    {
        AddrType res_addr = {};
        for (size_t i = 0; i < Length; i++) {
            res_addr.data[i] = ElemType(-1);
        }
        return res_addr;
    }
    
    static AddrType PrefixMask (size_t prefix_bits)
    {
        AIPSTACK_ASSERT(prefix_bits <= Bits)
        
        AddrType res_addr;
        size_t elem_idx = 0;
        size_t bits_left = prefix_bits;
        
        while (bits_left >= ElemBits) {
            res_addr.data[elem_idx++] = ElemType(-1);
            bits_left -= ElemBits;
        }
        
        if (bits_left > 0) {
            ElemType mask = ~((ElemType(1) << (ElemBits - bits_left)) - 1);
            res_addr.data[elem_idx++] = mask;
        }
        
        while (elem_idx < Length) {
            res_addr.data[elem_idx++] = 0;
        }
        
        return res_addr;
    }
    
    template <size_t PrefixBits>
    static constexpr AddrType PrefixMask ()
    {
        static_assert(PrefixBits <= Bits, "");
        
        AddrType res_addr = {};
        size_t elem_idx = 0;
        size_t bits_left = PrefixBits;
        
        while (bits_left >= ElemBits) {
            res_addr.data[elem_idx++] = ElemType(-1);
            bits_left -= ElemBits;
        }
        
        if (bits_left > 0) {
            ElemType mask = ~((ElemType(1) << (ElemBits - bits_left)) - 1);
            res_addr.data[elem_idx++] = mask;
        }
        
        while (elem_idx < Length) {
            res_addr.data[elem_idx++] = 0;
        }
        
        return res_addr;
    }
    
    static constexpr AddrType FromBytes (uint8_t const bytes[IpGenericAddr::Size])
    {
        static_assert(IpGenericAddr::Size == 4, "");
        AddrType addr = {};
        size_t byte_idx = 0;
        for (size_t elem_idx = 0; elem_idx < Length; elem_idx++) {
            for (size_t i = 0; i < IpGenericAddr::ElemSize; i++) {
                addr.data[elem_idx] |=
                    ElemType(bytes[byte_idx]) << (8 * (IpGenericAddr::ElemSize - 1 - i));
                byte_idx++;
            }
        }
        return addr;
    }
    
    static constexpr AddrType Join (
        IpGenericAddr const &mask, IpGenericAddr const &first, IpGenericAddr const &second)
    {
        return (first & mask) | (second & ~mask);
    }
    
    bool isZero () const
    {
        return *this == IpGenericAddr::ZeroAddr();
    }
    
    bool isAllOnes () const
    {
        return *this == IpGenericAddr::AllOnesAddr();
    }
    
    template <typename Func>
    constexpr AddrType bitwiseOp (IpGenericAddr const &other, Func func) const
    {
        AddrType res = {};
        for (size_t i = 0; i < Length; i++) {
            res.data[i] = func(this->data[i], other.data[i]);
        }
        return res;
    }
    
    template <typename Func>
    constexpr AddrType bitwiseOp (Func func) const
    {
        AddrType res = {};
        for (size_t i = 0; i < Length; i++) {
            res.data[i] = func(this->data[i]);
        }
        return res;
    }
    
    constexpr AddrType operator& (IpGenericAddr const &other) const
    {
        return bitwiseOp(other, [](ElemType x, ElemType y) { return x & y; });
    }
    
    constexpr AddrType operator| (IpGenericAddr const &other) const
    {
        return bitwiseOp(other, [](ElemType x, ElemType y) { return x | y; });
    }
    
    constexpr AddrType operator~ () const
    {
        return bitwiseOp([](ElemType x) { return ~x; });
    }
    
    constexpr size_t countLeadingOnes () const
    {
        size_t leading_ones = 0;
        for (size_t elem_idx = 0; elem_idx < Length; elem_idx++) {
            ElemType elem = this->data[elem_idx];
            for (size_t bit_idx = ElemBits - 1; bit_idx != size_t(-1); bit_idx--) {
                if ((elem & (ElemType(1) << bit_idx)) == 0) {
                    return leading_ones;
                }
                leading_ones++;
            }
        }
        return leading_ones;
    }

    template <size_t ByteIndex>
    constexpr uint8_t getByte () const
    {
        static_assert(ByteIndex < IpGenericAddr::Size, "");

        size_t elem_idx = ByteIndex / IpGenericAddr::ElemSize;
        size_t elem_byte_idx = ByteIndex % IpGenericAddr::ElemSize;

        ElemType elem = this->data[elem_idx];
        return (elem >> (8 * (IpGenericAddr::ElemSize - 1 - elem_byte_idx))) & 0xFF;
    }
};

class Ip4Addr : public IpGenericAddr<Ip4Addr, uint32_t, 1>
{
public:
    using IpGenericAddr::FromBytes;
    
    static constexpr Ip4Addr FromBytes (uint8_t n1, uint8_t n2, uint8_t n3, uint8_t n4)
    {
        uint8_t bytes[] = {n1, n2, n3, n4};
        return IpGenericAddr::FromBytes(bytes);
    }

    bool isMulticast() const
    {
        return (*this & Ip4Addr::FromBytes(0xF0, 0, 0, 0))
            == Ip4Addr::FromBytes(0xE0, 0, 0, 0);
    }
    
    bool isAllOnesOrMulticast () const
    {
        return isAllOnes() || isMulticast();
    }
};

/**
 * A pair of local and remote IPv4 addresses.
 */
struct Ip4Addrs {
    /**
     * Local address.
     */
    Ip4Addr local_addr;
    /**
     * Remote address.
     */
    Ip4Addr remote_addr;
};

/**
 * Unsigned 16-bit integer type used for port numbers.
 */
using PortNum = uint16_t;

}

#endif
