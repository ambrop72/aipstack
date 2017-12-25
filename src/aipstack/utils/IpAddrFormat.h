/*
 * Copyright (c) 2017 Ambroz Bizjak
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

#ifndef AIPSTACK_IP_ADDR_UTILS_H
#define AIPSTACK_IP_ADDR_UTILS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <aipstack/infra/MemRef.h>
#include <aipstack/proto/IpAddr.h>

namespace AIpStack {

/**
 * @ingroup misc
 * @defgroup ip-addr-format IP address formatting
 * @brief Utilities for formatting and parsing IP addresses
 * 
 * @{
 */

/**
 * Maximum number of characters that @ref FormatIpAddr may write including the null
 * terminator.
 */
static size_t const MaxIp4AddrPrintLen = 16;

/**
 * Format an IPv4 address to dot-decimal notation.
 * 
 * This generates the representation "N.N.N.N" where each N is a decimal representation of
 * the corresponding byte with no redundant leading zeros (zero is represented as "0").
 * 
 * @param addr IPv4 address to be formatted.
 * @param out_str Pointer to where the result will be written including a null terminator.
 *        It must not be null and there must be at least @ref MaxIp4AddrPrintLen bytes
 *        available.
 */
inline void FormatIpAddr (Ip4Addr addr, char *out_str)
{
    ::sprintf(out_str, "%d.%d.%d.%d",
        int(addr.getByte<0>()), int(addr.getByte<1>()),
        int(addr.getByte<2>()), int(addr.getByte<3>()));
}

/**
 * Parse an IPv4 address in dot-decimal notation.
 * 
 * This accepts any representation "N.N.N.N" where each N is a decimal representation of an
 * integer between 0 and 255 using between 1 and 3 decimal digits (and no other characters).
 * Inputs not satisfying this format will be rejected (this includes inputs with invalid
 * trailing data such as "1.2.3.4x").
 * 
 * @param str Pointer to input data and length, wrapped in @ref MemRef (the pointer may be
 *        null if the length is 0).
 * @param out_addr On success, is set to the parsed IP address (not changed on failure).
 * @return True on success, false on failure.
 */
inline bool ParseIpAddr (MemRef str, Ip4Addr &out_addr)
{
    uint8_t bytes[4];

    for (int i = 0; i < 4; i++) {
        if (i > 0) {
            if (str.len < 1 || str.ptr[0] != '.') {
                return false;
            }
            str = str.subFrom(1);
        }

        int byte_val = 0;

        for (int j = 0; j < 3; j++) {
            if (str.len < 1 || str.ptr[0] == '.') {
                if (j == 0) {
                    return false;
                }
                break;
            }

            char ch = str.ptr[0];
            str = str.subFrom(1);

            unsigned char digit_val = (unsigned char)ch - (unsigned char)'0';
            if (digit_val > 9) {
                return false;
            }

            byte_val = (10 * byte_val) + digit_val;
        }

        if (byte_val > 255) {
            return false;
        }

        bytes[i] = byte_val;
    }

    if (str.len > 0) {
        return false;
    }
    
    out_addr = Ip4Addr::FromBytes(bytes);
    return true;
}

/** @} */

}

#endif
