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

#ifndef AIPSTACK_ICMP4_PROTO_H
#define AIPSTACK_ICMP4_PROTO_H

#include <cstdint>
#include <array>

#include <aipstack/infra/Struct.h>

namespace AIpStack {

using Icmp4RestType = std::array<char, 4>;

enum class Icmp4Type : std::uint8_t {
    EchoReply   = 0,
    EchoRequest = 8,
    DestUnreach = 3,
};

enum class Icmp4Code : std::uint8_t {
    Zero                   = 0,
    DestUnreachPortUnreach = 3,
    DestUnreachFragNeeded  = 4,
};

AIPSTACK_DEFINE_STRUCT(Icmp4Header,
    (Type,   Icmp4Type)
    (Code,   Icmp4Code)
    (Chksum, std::uint16_t)
    (Rest,   Icmp4RestType)
)

inline std::uint16_t Icmp4GetMtuFromRest (Icmp4RestType rest) {
    return ReadSingleField<std::uint16_t>(rest.data() + 2);
}

}

#endif
