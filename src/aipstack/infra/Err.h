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

#ifndef AIPSTACK_ERR_H
#define AIPSTACK_ERR_H

#include <cstdint>

namespace AIpStack {

/**
 * @ingroup infra
 * @defgroup error Error Codes
 * @brief Error codes used throughout the stack.
 * @{
 */

/**
 * Error code enumeration used in various places, especially for sending packets.
 */
enum class IpErr : std::uint8_t {
    Success             = 0, /**< The operation was successful. */
    ArpQueryInProgress  = 1, /**< An ARP query is in progress and needs to complete. */
    NoHeaderSpace       = 2, /**< Insufficient header space is available in the buffer. */
    OutputBufferFull    = 3, /**< The transmit buffer of the interface is full. */
    NoHardwareRoute     = 4, /**< Could not determine the hardware address to send to. */
    NoIpRoute           = 5, /**< Could not find an IP route for the packet. */
    PacketTooLarge      = 6, /**< The packet exceeds the MTU of the interface. */
    NoPortAvailable     = 7, /**< A local port could not be allocated. */
    NoPcbAvailable      = 8, /**< A TCP PCB structure could be allocated. */
    NoMtuEntryAvailable = 9, /**< An IP MTU reference could not be allocated. */
    FragmentationNeeded = 10, /**< IP fragmentation is needed but not permitted. */
    HardwareError       = 11, /**< An unexpected hardware problem has occured. */
    LinkDown            = 12, /**< The link is down for the network interface. */
    BroadcastRejected   = 13, /**< Sending to a broadcast address was not allowed. */
    NonLocalSrc         = 14, /**< Sending from a non-local address was not allowed. */
    AddrInUse           = 15  /**< Address is already in use. */
};

/** @} */

}

#endif
