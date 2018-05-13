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

#ifndef AIPSTACK_TCP4_PROTO_H
#define AIPSTACK_TCP4_PROTO_H

#include <cstdint>
#include <cstddef>

#include <aipstack/infra/Struct.h>
#include <aipstack/proto/Ip4Proto.h>

namespace AIpStack {

AIPSTACK_DEFINE_STRUCT(Tcp4Header,
    (SrcPort,     std::uint16_t)
    (DstPort,     std::uint16_t)
    (SeqNum,      std::uint32_t)
    (AckNum,      std::uint32_t)
    (OffsetFlags, std::uint16_t)
    (WindowSize,  std::uint16_t)
    (Checksum,    std::uint16_t)
    (UrgentPtr,   std::uint16_t)
)

namespace Tcp4Flags {
    static std::uint16_t const Fin = std::uint16_t(1) << 0;
    static std::uint16_t const Syn = std::uint16_t(1) << 1;
    static std::uint16_t const Rst = std::uint16_t(1) << 2;
    static std::uint16_t const Psh = std::uint16_t(1) << 3;
    static std::uint16_t const Ack = std::uint16_t(1) << 4;
    static std::uint16_t const Urg = std::uint16_t(1) << 5;
    static std::uint16_t const Ece = std::uint16_t(1) << 6;
    static std::uint16_t const Cwr = std::uint16_t(1) << 7;
    static std::uint16_t const Ns  = std::uint16_t(1) << 8;

    static std::uint16_t const BasicFlags = Fin|Syn|Rst|Ack;
    static std::uint16_t const SeqFlags = Fin|Syn;
}

static int const TcpOffsetShift = 12;

enum class TcpOption : std::uint8_t {
    End = 0,
    Nop = 1,
    MSS = 2,
    WndScale = 3,
};

static std::size_t const Ip4TcpHeaderSize = Ip4Header::Size + Tcp4Header::Size;

}

#endif
