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

static std::uint16_t const Tcp4FlagFin = std::uint16_t(1) << 0;
static std::uint16_t const Tcp4FlagSyn = std::uint16_t(1) << 1;
static std::uint16_t const Tcp4FlagRst = std::uint16_t(1) << 2;
static std::uint16_t const Tcp4FlagPsh = std::uint16_t(1) << 3;
static std::uint16_t const Tcp4FlagAck = std::uint16_t(1) << 4;
static std::uint16_t const Tcp4FlagUrg = std::uint16_t(1) << 5;
static std::uint16_t const Tcp4FlagEce = std::uint16_t(1) << 6;
static std::uint16_t const Tcp4FlagCwr = std::uint16_t(1) << 7;
static std::uint16_t const Tcp4FlagNs  = std::uint16_t(1) << 8;

static std::uint16_t const Tcp4BasicFlags = Tcp4FlagFin|Tcp4FlagSyn|Tcp4FlagRst|Tcp4FlagAck;
static std::uint16_t const Tcp4SeqFlags = Tcp4FlagFin|Tcp4FlagSyn;

static int const TcpOffsetShift = 12;

static std::uint8_t const TcpOptionEnd = 0;
static std::uint8_t const TcpOptionNop = 1;
static std::uint8_t const TcpOptionMSS = 2;
static std::uint8_t const TcpOptionWndScale = 3;

static std::size_t const Ip4TcpHeaderSize = Ip4Header::Size + Tcp4Header::Size;

}

#endif
