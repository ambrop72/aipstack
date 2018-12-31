/*
 * Copyright (c) 2018 Ambroz Bizjak
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

#ifndef AIPSTACK_TCP_SEQ_NUM_H
#define AIPSTACK_TCP_SEQ_NUM_H

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include <aipstack/infra/Struct.h>

namespace AIpStack {

using TcpSeqInt = std::uint32_t;

class TcpSeqNum {
public:
    inline static constexpr std::size_t Size = 4;

private:
    inline static constexpr TcpSeqInt MSB_Value = TcpSeqInt(1) << 31;

private:
    TcpSeqInt m_value;

public:
    inline constexpr TcpSeqNum () :
        m_value(0)
    {}

    inline constexpr explicit TcpSeqNum (TcpSeqInt value) :
        m_value(value)
    {}

    inline constexpr TcpSeqInt value () const {
        return m_value;
    }

    inline static TcpSeqNum readBinary (char const *src) {
        return TcpSeqNum(ReadSingleField<TcpSeqInt>(src));
    }

    inline void writeBinary (char *dst) const {
        WriteSingleField<TcpSeqInt>(dst, value());
    }

    inline constexpr bool operator== (TcpSeqNum other) const {
        return value() == other.value();
    }

    inline constexpr bool operator!= (TcpSeqNum other) const {
        return value() != other.value();
    }

    inline constexpr bool ref_lte (TcpSeqNum op1, TcpSeqNum op2) const {
        return op1 - *this <= op2 - *this;
    }
    
    inline constexpr bool ref_lt (TcpSeqNum op1, TcpSeqNum op2) const {
        return op1 - *this < op2 - *this;
    }
    
    inline constexpr bool mod_lt (TcpSeqNum other) const {
        return *this - other >= MSB_Value;
    }

    inline constexpr TcpSeqInt operator- (TcpSeqNum other) const {
        return TcpSeqInt(value() - other.value());
    }

    template<typename IntType,
        typename = std::enable_if_t<std::is_unsigned<IntType>::value>>
    inline constexpr TcpSeqNum operator+ (IntType int_val) const {
        return TcpSeqNum(TcpSeqInt(value() + int_val));
    }

    template<typename IntType,
        typename = std::enable_if_t<std::is_unsigned<IntType>::value>>
    inline constexpr TcpSeqNum operator- (IntType int_val) const {
        return TcpSeqNum(TcpSeqInt(value() - int_val));
    }

    template<typename IntType,
        typename = std::enable_if_t<std::is_unsigned<IntType>::value>>
    inline constexpr TcpSeqNum & operator+= (IntType int_val) {
        *this = operator+(int_val);
        return *this;
    }

    template<typename IntType,
        typename = std::enable_if_t<std::is_unsigned<IntType>::value>>
    inline constexpr TcpSeqNum & operator-= (IntType int_val) {
        *this = operator-(int_val);
        return *this;
    }
};

#ifndef IN_DOXYGEN
template<>
struct StructTypeHandler<TcpSeqNum, void> {
    using Handler = StructConventionalTypeHandler<TcpSeqNum>;
};
#endif

}

#endif
