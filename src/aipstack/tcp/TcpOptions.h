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

#ifndef AIPSTACK_TCP_OPTIONS_H
#define AIPSTACK_TCP_OPTIONS_H

#include <cstdint>
#include <cstddef>

#include <aipstack/misc/Assert.h>
#include <aipstack/misc/EnumUtils.h>
#include <aipstack/misc/EnumBitfieldUtils.h>
#include <aipstack/infra/Buf.h>
#include <aipstack/infra/Struct.h>
#include <aipstack/proto/Tcp4Proto.h>

namespace AIpStack {

// TCP options flags used in TcpOptions options field.
enum class TcpOptionFlags : std::uint8_t {
    Mss      = 1 << 0,
    WndScale = 1 << 1,
};
AIPSTACK_ENUM_BITFIELD(TcpOptionFlags)

// Container for TCP options that we care about.
struct TcpOptions {
    TcpOptionFlags options;
    std::uint8_t wnd_scale;
    std::uint16_t mss;
};

namespace TcpOptionWriteLen {
    inline constexpr std::size_t MSS = 4;
    inline constexpr std::size_t WndScale = 4;
}

inline constexpr std::size_t MaxTcpOptionsWriteLen =
    TcpOptionWriteLen::MSS + TcpOptionWriteLen::WndScale;

inline void ParseTcpOptions (IpBufRef buf, TcpOptions &out_opts)
{
    // Clear options flags. Below we will set flags for options that we find.
    out_opts.options = TcpOptionFlags(0);
    
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
                out_opts.options |= TcpOptionFlags::Mss;
                out_opts.mss = ReadSingleField<std::uint16_t>(opt_data);
            } break;
            
            // Window Scale
            case TcpOption::WndScale: {
                if (opt_data_len != 1) {
                    goto skip_option;
                }
                std::uint8_t value = std::uint8_t(buf.takeByte());
                out_opts.options |= TcpOptionFlags::WndScale;
                out_opts.wnd_scale = value;
            } break;
            
            // Unknown option (also used to handle bad options).
            skip_option:
            default: {
                buf.skipBytes(opt_data_len);
            } break;
        }
    }
}

inline std::uint8_t CalcTcpOptionsLength (TcpOptions const &tcp_opts)
{
    std::uint8_t opts_len = 0;
    if ((tcp_opts.options & TcpOptionFlags::Mss) != Enum0) {
        opts_len += TcpOptionWriteLen::MSS;
    }
    if ((tcp_opts.options & TcpOptionFlags::WndScale) != Enum0) {
        opts_len += TcpOptionWriteLen::WndScale;
    }
    AIPSTACK_ASSERT(opts_len <= MaxTcpOptionsWriteLen);
    AIPSTACK_ASSERT(opts_len % 4 == 0); // caller needs padding to 4-byte alignment
    return opts_len;
}

inline void WriteTcpOptions (TcpOptions const &tcp_opts, char *out)
{
    if ((tcp_opts.options & TcpOptionFlags::Mss) != Enum0) {
        WriteSingleField<std::uint8_t >(out + 0, AsUnderlying(TcpOption::MSS));
        WriteSingleField<std::uint8_t >(out + 1, /*length=*/4);
        WriteSingleField<std::uint16_t>(out + 2, tcp_opts.mss);
        out += TcpOptionWriteLen::MSS;
    }

    if ((tcp_opts.options & TcpOptionFlags::WndScale) != Enum0) {
        WriteSingleField<std::uint8_t>(out + 0, AsUnderlying(TcpOption::Nop));
        WriteSingleField<std::uint8_t>(out + 1, AsUnderlying(TcpOption::WndScale));
        WriteSingleField<std::uint8_t>(out + 2, /*length=*/3);
        WriteSingleField<std::uint8_t>(out + 3, tcp_opts.wnd_scale);
        out += TcpOptionWriteLen::WndScale;
    }
}

}

#endif
