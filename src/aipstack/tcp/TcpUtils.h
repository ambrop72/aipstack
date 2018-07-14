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

#ifndef AIPSTACK_TCP_UTILS_H
#define AIPSTACK_TCP_UTILS_H

#include <cstdint>
#include <cstddef>

#include <aipstack/misc/MinMax.h>
#include <aipstack/proto/Tcp4Proto.h>
#include <aipstack/ip/IpAddr.h>
#include <aipstack/tcp/TcpSeqNum.h>
#include <aipstack/tcp/TcpOptions.h>

namespace AIpStack {

class TcpUtils {
public:
    // Container for data in the TCP header (used both in RX and TX).
    struct TcpSegMeta {
        PortNum local_port;
        PortNum remote_port;
        TcpSeqNum seq_num;
        TcpSeqNum ack_num;
        std::uint16_t window_size;
        Tcp4Flags flags;
        TcpOptions *opts; // not used for RX (undefined), may be null for TX
    };
    
    static inline std::size_t tcplen (Tcp4Flags flags, std::size_t tcp_data_len)
    {
        return tcp_data_len + ((flags & Tcp4Flags::SeqFlags) != EnumZero);
    }

    template <std::uint16_t MinAllowedMss>
    static bool calc_snd_mss (std::uint16_t iface_mss,
                              TcpOptions const &tcp_opts, std::uint16_t *out_mss)
    {
        std::uint16_t req_mss = ((tcp_opts.options & TcpOptionFlags::MSS) != 0) ?
            tcp_opts.mss : 536;
        std::uint16_t mss = MinValue(iface_mss, req_mss);
        if (mss < MinAllowedMss) {
            return false;
        }
        *out_mss = mss;
        return true;
    }
    
    static TcpSeqInt calc_initial_cwnd (std::uint16_t snd_mss)
    {
        if (snd_mss > 2190) {
            return 2u * TcpSeqInt(snd_mss);
        }
        else if (snd_mss > 1095) {
            return 3u * TcpSeqInt(snd_mss);
        }
        else {
            return 4u * TcpSeqInt(snd_mss);
        }
    }
};

}

#endif
