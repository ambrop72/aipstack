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
#include <limits>

#include <aipstack/misc/Assert.h>
#include <aipstack/misc/MinMax.h>
#include <aipstack/infra/Buf.h>
#include <aipstack/infra/Struct.h>
#include <aipstack/proto/Tcp4Proto.h>
#include <aipstack/ip/IpAddr.h>
#include <aipstack/tcp/TcpSeqNum.h>

namespace AIpStack {

class TcpUtils {
public:
    struct TcpOptions;
    
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
    
    // TCP options flags used in TcpOptions options field.
    struct OptionFlags { enum : std::uint8_t {
        MSS       = 1 << 0,
        WND_SCALE = 1 << 1,
    }; };
    
    // Container for TCP options that we care about.
    struct TcpOptions {
        std::uint8_t options;
        std::uint8_t wnd_scale;
        std::uint16_t mss;
    };
    
    static inline std::size_t tcplen (Tcp4Flags flags, std::size_t tcp_data_len)
    {
        return tcp_data_len + ((flags & Tcp4Flags::SeqFlags) != EnumZero);
    }
        
    static inline void parse_options (IpBufRef buf, TcpOptions *out_opts)
    {
        // Clear options flags. Below we will set flags for options that we find.
        out_opts->options = 0;
        
        while (buf.tot_len > 0) {
            // Read the option kind.
            TcpOption kind = TcpOption(std::uint8_t(buf.takeByte()));
            
            // Hanlde end option and nop option.
            if (kind == TcpOption::End) {
                break;
            }
            else if (kind == TcpOption::Nop) {
                continue;
            }
            
            // Read the option length.
            if (buf.tot_len == 0) {
                break;
            }
            std::uint8_t length = std::uint8_t(buf.takeByte());
            
            // Check the option length.
            if (length < 2) {
                break;
            }
            std::uint8_t opt_data_len = length - 2;
            if (buf.tot_len < opt_data_len) {
                break;
            }
            
            // Handle different options, consume option data.
            switch (kind) {
                // Maximum Segment Size
                case TcpOption::MSS: {
                    if (opt_data_len != 2) {
                        goto skip_option;
                    }
                    char opt_data[2];
                    buf.takeBytes(opt_data_len, opt_data);
                    out_opts->options |= OptionFlags::MSS;
                    out_opts->mss = ReadSingleField<std::uint16_t>(opt_data);
                } break;
                
                // Window Scale
                case TcpOption::WndScale: {
                    if (opt_data_len != 1) {
                        goto skip_option;
                    }
                    std::uint8_t value = std::uint8_t(buf.takeByte());
                    out_opts->options |= OptionFlags::WND_SCALE;
                    out_opts->wnd_scale = value;
                } break;
                
                // Unknown option (also used to handle bad options).
                skip_option:
                default: {
                    buf.skipBytes(opt_data_len);
                } break;
            }
        }
    }
    
    static std::size_t const OptWriteLenMSS = 4;
    static std::size_t const OptWriteLenWndScale = 4;
    
    static std::size_t const MaxOptionsWriteLen = OptWriteLenMSS + OptWriteLenWndScale;
    
    static inline std::uint8_t calc_options_len (TcpOptions const &tcp_opts)
    {
        std::uint8_t opts_len = 0;
        if ((tcp_opts.options & OptionFlags::MSS) != 0) {
            opts_len += OptWriteLenMSS;
        }
        if ((tcp_opts.options & OptionFlags::WND_SCALE) != 0) {
            opts_len += OptWriteLenWndScale;
        }
        AIPSTACK_ASSERT(opts_len <= MaxOptionsWriteLen)
        AIPSTACK_ASSERT(opts_len % 4 == 0) // caller needs padding to 4-byte alignment
        return opts_len;
    }
    
    static inline void write_options (TcpOptions const &tcp_opts, char *out)
    {
        if ((tcp_opts.options & OptionFlags::MSS) != 0) {
            WriteSingleField<std::uint8_t >(out + 0, AsUnderlying(TcpOption::MSS));
            WriteSingleField<std::uint8_t >(out + 1, 4);
            WriteSingleField<std::uint16_t>(out + 2, tcp_opts.mss);
            out += OptWriteLenMSS;
        }

        if ((tcp_opts.options & OptionFlags::WND_SCALE) != 0) {
            WriteSingleField<std::uint8_t>(out + 0, AsUnderlying(TcpOption::Nop));
            WriteSingleField<std::uint8_t>(out + 1, AsUnderlying(TcpOption::WndScale));
            WriteSingleField<std::uint8_t>(out + 2, 3);
            WriteSingleField<std::uint8_t>(out + 3, tcp_opts.wnd_scale);
            out += OptWriteLenWndScale;
        }
    }
    
    template <std::uint16_t MinAllowedMss>
    static bool calc_snd_mss (std::uint16_t iface_mss,
                              TcpOptions const &tcp_opts, std::uint16_t *out_mss)
    {
        std::uint16_t req_mss = ((tcp_opts.options & OptionFlags::MSS) != 0) ?
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
    
    /**
     * Determine if x is in the half-open interval (start, start+length].
     * IntType must be an unsigned integer type.
     * 
     * Note that the interval is understood in terms of modular
     * arithmetic, so if a+b is not representable in this type
     * the result may not be what you expect.
     * 
     * Thanks to Simon Stienen for this most efficient formula.
     */
    template <typename IntType>
    inline static bool InOpenClosedIntervalStartLen (
        IntType start, IntType length, IntType x)
    {
        static_assert(std::numeric_limits<IntType>::is_integer, "");
        static_assert(!std::numeric_limits<IntType>::is_signed, "");
        
        return IntType(x + ~start) < length;
    }
};

}

#endif
