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

#ifndef AIPSTACK_TCP_PCB_FLAGS_H
#define AIPSTACK_TCP_PCB_FLAGS_H

#include <cstdint>

#include <aipstack/misc/EnumBitfieldUtils.h>

namespace AIpStack {

using TcpPcbFlagsBaseType = std::uint16_t;

static constexpr int TcpPcbFlagsBits = 14;

enum class TcpPcbFlags : TcpPcbFlagsBaseType {
    // ACK is needed; used in input processing
    ACK_PENDING = TcpPcbFlagsBaseType(1) << 0,
    // pcb_output_active/pcb_output_abandoned should be called at the end of
    // input processing. This flag must imply state().canOutput() and
    // pcb_has_snd_outstanding at the point in pcb_input where it is checked.
    // Any change that would break this implication must clear the flag.
    OUT_PENDING = TcpPcbFlagsBaseType(1) << 1,
    // A FIN was sent at least once and is included in snd_nxt
    FIN_SENT    = TcpPcbFlagsBaseType(1) << 2,
    // A FIN is to queued for sending
    FIN_PENDING = TcpPcbFlagsBaseType(1) << 3,
    // Round-trip-time is being measured
    RTT_PENDING = TcpPcbFlagsBaseType(1) << 4,
    // Round-trip-time is not in initial state
    RTT_VALID   = TcpPcbFlagsBaseType(1) << 5,
    // cwnd has been increaded by snd_mss this round-trip
    CWND_INCRD  = TcpPcbFlagsBaseType(1) << 6,
    // A segment has been retransmitted and not yet acked
    RTX_ACTIVE  = TcpPcbFlagsBaseType(1) << 7,
    // The recover variable valid (and >=snd_una)
    RECOVER     = TcpPcbFlagsBaseType(1) << 8,
    // If rtx_timer is running it is for idle timeout
    IDLE_TIMER  = TcpPcbFlagsBaseType(1) << 9,
    // Window scaling is used
    WND_SCALE   = TcpPcbFlagsBaseType(1) << 10,
    // Current cwnd is the initial cwnd
    CWND_INIT   = TcpPcbFlagsBaseType(1) << 11,
    // If OutputTimer is set it is for OutputRetry*Ticks
    OUT_RETRY   = TcpPcbFlagsBaseType(1) << 12,
    // rcv_ann_wnd needs update before sending a segment, implies con != nullptr
    RCV_WND_UPD = TcpPcbFlagsBaseType(1) << 13,
    // NOTE: Currently no more bits are available, see TcpPcb::flags.
};
AIPSTACK_ENUM_BITFIELD(TcpPcbFlags)

}

#endif
