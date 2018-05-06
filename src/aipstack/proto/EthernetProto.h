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

#ifndef AIPSTACK_ETHERNET_PROTO_H
#define AIPSTACK_ETHERNET_PROTO_H

#include <cstdint>
#include <cstddef>

#include <aipstack/infra/Struct.h>

namespace AIpStack {

class MacAddr : public StructByteArray<6>
{
public:
    static inline constexpr MacAddr ZeroAddr ()
    {
        return MacAddr{};
    }
    
    static inline constexpr MacAddr BroadcastAddr ()
    {
        MacAddr result = {};
        for (std::size_t i = 0; i < MacAddr::Size; i++) {
            result.data[i] = 0xFF;
        }
        return result;
    }
    
    static inline constexpr MacAddr Make (std::uint8_t b1, std::uint8_t b2, std::uint8_t b3,
                                          std::uint8_t b4, std::uint8_t b5, std::uint8_t b6)
    {
        MacAddr result = {};
        result.data[0] = b1;
        result.data[1] = b2;
        result.data[2] = b3;
        result.data[3] = b4;
        result.data[4] = b5;
        result.data[5] = b6;
        return result;
    }
    
    inline static MacAddr decode (char const *bytes)
    {
        return StructByteArray<6>::template decodeTo<MacAddr>(bytes);
    }
};

AIPSTACK_DEFINE_STRUCT(EthHeader,
    (DstMac,  MacAddr)
    (SrcMac,  MacAddr)
    (EthType, std::uint16_t)
)

static std::uint16_t const EthTypeIpv4 = UINT16_C(0x0800);
static std::uint16_t const EthTypeArp  = UINT16_C(0x0806);

}

#endif
