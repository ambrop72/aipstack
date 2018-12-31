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

#ifndef AIPSTACK_IP4_PROTO_H
#define AIPSTACK_IP4_PROTO_H

#include <cstdint>
#include <cstddef>

#include <aipstack/infra/Struct.h>
#include <aipstack/ip/IpAddr.h>
#include <aipstack/misc/EnumBitfieldUtils.h>

namespace AIpStack {

enum class Ip4Protocol : std::uint8_t {
    Icmp = 1,
    Tcp  = 6,
    Udp  = 17,
};

enum class Ip4Flags : std::uint16_t {
    DF = std::uint16_t(1) << 14,
    MF = std::uint16_t(1) << 13,
    OffsetMask = 0x1fff,
};
AIPSTACK_ENUM_BITFIELD(Ip4Flags)

AIPSTACK_DEFINE_STRUCT(Ip4Header,
    (VersionIhlDscpEcn, std::uint16_t)
    (TotalLen,          std::uint16_t)
    (Ident,             std::uint16_t)
    (FlagsOffset,       Ip4Flags)
    (Ttl,               std::uint8_t)
    (Proto,             Ip4Protocol)
    (HeaderChksum,      std::uint16_t)
    (SrcAddr,           Ip4Addr)
    (DstAddr,           Ip4Addr)
)

inline constexpr int Ip4VersionShift = 4;
inline constexpr std::uint8_t Ip4IhlMask = 0xF;

inline constexpr std::size_t Ip4MaxHeaderSize = 60;

// The full datagram size which every internet destination must be
// be able to receive either in one piece or in fragments (RFC 791 page 25).
inline constexpr std::uint16_t Ip4RequiredRecvSize = 576;

inline std::uint16_t Ip4RoundFragLen (std::uint8_t header_length, std::uint16_t mtu) {
    return header_length + (((mtu - header_length) / 8) * 8);
}

}

#endif
