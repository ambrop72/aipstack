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

#ifndef AIPSTACK_MAC_ADDR_H
#define AIPSTACK_MAC_ADDR_H

#include <cstdint>
#include <cstddef>
#include <array>

#include <aipstack/infra/Struct.h>

namespace AIpStack {

/**
 * @addtogroup eth-ip-iface
 * @{
 */

class MacAddr {
public:
    static constexpr std::size_t Size = 6;

    using ValueArray = std::array<std::uint8_t, Size>;
    
private:
    ValueArray m_value;

public:
    inline constexpr MacAddr () :
        m_value{{0, 0, 0, 0, 0, 0}}
    {}

    inline explicit constexpr MacAddr (ValueArray value) :
        m_value(value)
    {}

    inline explicit constexpr MacAddr (
        std::uint8_t b1, std::uint8_t b2, std::uint8_t b3,
        std::uint8_t b4, std::uint8_t b5, std::uint8_t b6)
    :
        m_value{{b1, b2, b3, b4, b5, b6}}
    {}

    inline constexpr ValueArray value () const {
        return m_value;
    }

    inline bool operator== (MacAddr other) const {
        return value() == other.value();
    }
    
    inline bool operator!= (MacAddr other) const {
        return value() != other.value();
    }
    
    inline bool operator< (MacAddr other) const {
        return value() < other.value();
    }
    
    inline bool operator<= (MacAddr other) const {
        return value() <= other.value();
    }
    
    inline bool operator> (MacAddr other) const {
        return value() > other.value();
    }
    
    inline bool operator>= (MacAddr other) const {
        return value() >= other.value();
    }
    
    inline static constexpr MacAddr ZeroAddr () {
        return MacAddr(0, 0, 0, 0, 0, 0);
    }
    
    inline static constexpr MacAddr BroadcastAddr () {
        return MacAddr(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
    }
    
    inline static MacAddr readBinary (char const *src) {
        return MacAddr(ReadSingleField<ValueArray>(src));
    }

    inline void writeBinary (char *dst) const {
        WriteSingleField<ValueArray>(dst, value());
    }
};

#ifndef IN_DOXYGEN
template <>
struct StructTypeHandler<MacAddr, void> {
    using Handler = StructConventionalTypeHandler<MacAddr>;
};
#endif

/** @} */

}

#endif
