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

#ifndef AIPSTACK_IP_TCP_PROTO_CONSTANTS_H
#define AIPSTACK_IP_TCP_PROTO_CONSTANTS_H

#include <cstdint>

#include <aipstack/meta/BitsInInt.h>
#include <aipstack/meta/BitsInFloat.h>
#include <aipstack/misc/PowerOfTwo.h>
#include <aipstack/misc/Use.h>
#include <aipstack/misc/MinMax.h>
#include <aipstack/proto/Tcp4Proto.h>
#include <aipstack/ip/IpStack.h>
#include <aipstack/platform/PlatformFacade.h>
#include <aipstack/tcp/TcpSeqNum.h>

namespace AIpStack {

template <typename Arg>
class IpTcpProto_constants
{
    AIPSTACK_USE_TYPES(Arg, (PlatformImpl, StackArg))

    using Platform = PlatformFacade<PlatformImpl>;
    AIPSTACK_USE_TYPES(Platform, (TimeType))

    // Make sure the MinMTU permits an unfragmented TCP segment with some data.
    static_assert(IpStack<StackArg>::MinMTU >= Ip4TcpHeaderSize + 32, "");
    
public:
    // For retransmission time calculations we right-shift the TimeType
    // to obtain granularity between 1ms and 2ms.
    inline static constexpr int RttShift = BitsInFloat(1e-3 * Platform::TimeFreq);
    static_assert(RttShift >= 0, "");

    // The resulting frequency of such right-shifted time.
    inline static constexpr double RttTimeFreq =
        Platform::TimeFreq / PowerOfTwo<double>(RttShift);
    
    // We store such scaled times in 16-bit variables. This gives us a range of at least 65
    // seconds.
    using RttType = std::uint16_t;
    
    // For intermediate RTT results we need a larger type.
    using RttNextType = std::uint32_t;

    // Don't allow the remote host to lower the MSS beyond this.
    // NOTE: pcb_calc_snd_mss_from_pmtu relies on this definition.
    inline static constexpr std::uint16_t MinAllowedMss =
        IpStack<StackArg>::MinMTU - Ip4TcpHeaderSize;
    
    // Common flags passed to IpStack::sendIp4Dgram.
    // We disable fragmentation of TCP segments sent by us, due to PMTUD.
    inline static constexpr IpSendFlags TcpIpSendFlags = IpSendFlags::DontFragmentFlag;
    
    // Maximum theoreticaly possible send and receive window.
    inline static constexpr TcpSeqInt MaxWindow = 0x3fffffff;
    
    // Default window update threshold (overridable by setWindowUpdateThreshold).
    inline static constexpr TcpSeqInt DefaultWndAnnThreshold = 2700;
    
    // How old at most an ACK may be to be considered acceptable (MAX.SND.WND in RFC 5961).
    inline static constexpr TcpSeqInt MaxAckBefore = 0xFFFF;
    
    // SYN_RCVD state timeout.
    inline static constexpr TimeType SynRcvdTimeoutTicks     = 20.0  * Platform::TimeFreq;
    
    // SYN_SENT state timeout.
    inline static constexpr TimeType SynSentTimeoutTicks     = 30.0  * Platform::TimeFreq;
    
    // TIME_WAIT state timeout.
    inline static constexpr TimeType TimeWaitTimeTicks       = 120.0 * Platform::TimeFreq;
    
    // Timeout to abort connection after it has been abandoned.
    inline static constexpr TimeType AbandonedTimeoutTicks   = 30.0  * Platform::TimeFreq;
    
    // Time after the send buffer is extended to calling pcb_output.
    inline static constexpr TimeType OutputTimerTicks        = 0.0005 * Platform::TimeFreq;
    
    // Time to retry after sending failed with error IpErr::OutputBufferFull.
    inline static constexpr TimeType OutputRetryFullTicks    = 0.1 * Platform::TimeFreq;
    
    // Time to retry after sending failed with error other then IpErr::OutputBufferFull.
    inline static constexpr TimeType OutputRetryOtherTicks   = 2.0 * Platform::TimeFreq;
    
    // Initial retransmission time, before any round-trip-time measurement.
    inline static constexpr RttType InitialRtxTime           = 1.0 * RttTimeFreq;
    
    // Minimum retransmission time.
    inline static constexpr RttType MinRtxTime               = 0.25 * RttTimeFreq;
    
    // Maximum retransmission time (need care not to overflow RttType).
    inline static constexpr RttType MaxRtxTime =
        MinValue(double(TypeMax<RttType>()), 60. * RttTimeFreq);
    
    // Number of duplicate ACKs to trigger fast retransmit/recovery.
    inline static constexpr std::uint8_t FastRtxDupAcks = 3;
    
    // Maximum number of additional duplicate ACKs that will result in CWND increase.
    inline static constexpr std::uint8_t MaxAdditionaDupAcks = 32;
    
    // Number of bits needed to represent the maximum supported duplicate ACK count.
    inline static constexpr int DupAckBits =
        BitsInInt<FastRtxDupAcks + MaxAdditionaDupAcks>::Value;
    
    // Window scale shift count to send and use in outgoing ACKs.
    inline static constexpr std::uint8_t RcvWndShift = 6;
    static_assert(RcvWndShift <= 14, "");
    
    // Minimum amount to extend the receive window when a PCB is
    // abandoned before the FIN has been received.
    inline static constexpr TcpSeqInt MinAbandonRcvWndIncr = TypeMax<std::uint16_t>();
};

}

#endif
