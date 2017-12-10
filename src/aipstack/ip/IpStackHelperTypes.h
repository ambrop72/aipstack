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

#ifndef AIPSTACK_IPSTACK_HELPER_TYPES_H
#define AIPSTACK_IPSTACK_HELPER_TYPES_H

#include <stdint.h>
#include <stddef.h>

#include <aipstack/misc/EnumBitfieldUtils.h>
#include <aipstack/infra/Chksum.h>
#include <aipstack/proto/IpAddr.h>
#include <aipstack/proto/Ip4Proto.h>
#include <aipstack/proto/Icmp4Proto.h>
#include <aipstack/ip/hw/IpHwCommon.h>
#include <aipstack/platform/PlatformFacade.h>

namespace AIpStack {

#ifndef IN_DOXYGEN
template <typename> class IpIface;
#endif

/**
 * @addtogroup ip-stack
 * @{
 */

/**
 * Represents the IPv4 address configuration of a network interface.
 * 
 * Structures of this type are passed to @ref IpIface::setIp4Addr
 * and returned by @ref IpIface::getIp4Addr.
 */
struct IpIfaceIp4AddrSetting {
    /**
     * Whether an IP address is or should be assigned.
     * 
     * If this is false, then other members of this structure are meaningless.
     */
    bool present;
    
    /**
     * The subnet prefix length.
     */
    uint8_t prefix;
    
    /**
     * The IPv4 address.
     */
    Ip4Addr addr;
};

/**
 * Represents the IPv4 gateway configuration of a network interface.
 * 
 * Structures of this type are passed to @ref IpIface::setIp4Gateway
 * and returned by @ref IpIface::getIp4Gateway.
 */
struct IpIfaceIp4GatewaySetting {
    /**
     * Whether a gateway address is or should be assigned.
     * 
     * If this is false, then other members of this structure are meaningless.
     */
    bool present;
    
    /**
     * The gateway address.
     */
    Ip4Addr addr;
};

/**
 * Contains cached information about the IPv4 address configuration of a
 * network interface.
 * 
 * A pointer to a structure of this type can be obtained using
 * @ref IpIface::getIp4AddrsFromDriver. In addition to the IP address
 * and subnet prefix length, this structure contains the network mask,
 * network address and local broadcast address.
 */
struct IpIfaceIp4Addrs {
    /**
     * The IPv4 address.
     */
    Ip4Addr addr;
    
    /**
     * The network mask.
     */
    Ip4Addr netmask;
    
    /**
     * The network address.
     */
    Ip4Addr netaddr;
    
    /**
     * The local broadcast address.
     */
    Ip4Addr bcastaddr;
    
    /**
     * The subnet prefix length.
     */
    uint8_t prefix;
};

/**
 * Contains state reported by IP interface drivers to the IP stack.
 * 
 * Structures of this type are returned by @ref IpIface::getDriverState,
 * as well as by @ref IpIface::driverGetState as part of the driver
 * interface.
 */
struct IpIfaceDriverState {
    /**
     * Whether the link is up.
     */
    bool link_up;
};

/**
 * Contains definitions of flags as accepted by @ref AIpStack::IpStack::sendIp4Dgram
 * "IpStack::sendIp4Dgram" and @ref AIpStack::IpStack::prepareSendIp4Dgram
 * "IpStack::prepareSendIp4Dgram".
 * 
 * Operators provided by @ref AIPSTACK_ENUM_BITFIELD_OPS are available.
 */
enum class IpSendFlags : uint16_t {
    /**
     * Allow broadcast.
     * 
     * This flag is required in order to send to a local broadcast or all-ones address.
     * If it is set then sending to non-broadcast addresses is still allowed.
     */
    AllowBroadcastFlag = (uint16_t)1 << 0,

    /**
     * Allow sending from from a non-local address.
     * 
     * This flag is required in order to send using a source address that is not the
     * address of the outgoing network interface.
     */
    AllowNonLocalSrc = (uint16_t)1 << 1,

    /**
     * Do-not-fragment flag.
     * 
     * Using this flag will both prevent fragmentation of the outgoing
     * datagram as well as set the Dont-Fragment flag in the IP header.
     */
    DontFragmentFlag = Ip4FlagDF,

    /**
     * Mask of all flags which may be passed to send functions.
     */
    AllFlags = AllowBroadcastFlag|AllowNonLocalSrc|DontFragmentFlag,
};

#ifndef IN_DOXYGEN
AIPSTACK_ENUM_BITFIELD_OPS(IpSendFlags)
#endif

/**
 * Contains information about a received ICMP Destination Unreachable message.
 */
struct Ip4DestUnreachMeta {
    /**
     * The ICMP code.
     * 
     * For example, @ref Icmp4CodeDestUnreachFragNeeded may be of interest.
     */
    uint8_t icmp_code;
    
    /**
     * The "Rest of Header" part of the ICMP header (4 bytes).
     */
    Icmp4RestType icmp_rest;
};

/**
 * Encapsulates a pair of IPv4 TTL and protocol values.
 * 
 * These are encoded in a 16-bit unsigned integer in the same manner
 * as in the IPv4 header. That is, the TTL is stored in the higher
 * 8 bits and the protocol in the lower 8 bits.
 */
class Ip4TtlProto {
public:
    /**
     * The encoded TLL and protocol.
     */
    uint16_t value;
    
public:
    /**
     * Default constructor, leaves the \ref value uninitialized.
     */
    Ip4TtlProto () = default;
    
    /**
     * Constructor from an encoded TTL and protocol value.
     * 
     * @param ttl_proto Encoded TTL and protocol. The \ref value field
     *        will be initialized to this value.
     */
    constexpr inline Ip4TtlProto (uint16_t ttl_proto)
    : value(ttl_proto)
    {
    }
    
    /**
     * Constructor from separate TTL and protocol values.
     * 
     * @param ttl The TTL.
     * @param proto The protocol.
     */
    constexpr inline Ip4TtlProto (uint8_t ttl, uint8_t proto)
    : value(((uint16_t)ttl << 8) | proto)
    {
    }
    
    /**
     * Returns the TTL.
     * 
     * @return The TTL.
     */
    constexpr inline uint8_t ttl () const
    {
        return uint8_t(value >> 8);
    }
    
    /**
     * Returns the protocol.
     * 
     * @return The protocol.
     */
    constexpr inline uint8_t proto () const
    {
        return uint8_t(value);
    }
};

/**
 * Encapsulates interface information passed to the @ref IpIface constructor.
 */
struct IpIfaceInitInfo {
    /**
     * The Maximum Transmission Unit (MTU), including the IP header.
     * 
     * It must be at least @ref IpStack::MinMTU (this is an assert).
     */
    size_t ip_mtu = 0;
    
    /**
     * The type of the hardware-type-specific interface.
     * 
     * See @ref IpIface::getHwType for an explanation of the
     * hardware-type-specific interface mechanism. If no hardware-type-specific
     * interface is available, use @ref IpHwType::Undefined.
     */
    IpHwType hw_type = IpHwType::Undefined;
    
    /**
     * Pointer to the hardware-type-specific interface.
     * 
     * If @ref hw_type is @ref IpHwType::Undefined, use null. Otherwise this must
     * point to an instance of the hardware-type-specific interface class
     * corresponding to @ref hw_type.
     */
    void *hw_iface = nullptr;
};

/**
 * Encapsulates parameters passed to protocol handler constructors.
 * 
 * See @ref IpProtocolHandlerStub::IpProtocolHandlerStub for the documentation
 * of protocol handler construction.
 * 
 * @tparam TheIpStack The @ref IpStack class type.
 */
template <typename TheIpStack>
struct IpProtocolHandlerArgs {
    /**
     * The platform facade, as passed to the @ref IpStack::IpStack constructor.
     */
    PlatformFacade<typename TheIpStack::PlatformImpl> platform;
    
    /**
     * A pointer to the IP stack.
     */
    TheIpStack *stack;
};

/**
 * Encapsulates route information returned route functions.
 * 
 * Functions such as @ref IpStack::routeIp4 and @ref IpStack::routeIp4ForceIface will fill
 * in this structure. The result is only valid temporarily because it contains a pointer to
 * an interface, which could be removed.
 * 
 * @tparam TheIpStack The @ref IpStack class type.
 */
template <typename TheIpStack>
struct IpRouteInfoIp4 {
    /**
     * The interface to send through.
     */
    IpIface<TheIpStack> *iface;
    
    /**
     * The address of the next hop.
     */
    Ip4Addr addr;
};

/**
 * Encapsulates information about a received IPv4 datagram.
 * 
 * This is filled in by the stack and passed to the recvIp4Dgram function of
 * protocol handlers and also to @ref IpIfaceListener::recvIp4Dgram.
 * 
 * @tparam TheIpStack The @ref IpStack class type.
 */
template <typename TheIpStack>
struct IpRxInfoIp4 {
    /**
     * The source address.
     */
    Ip4Addr src_addr;
    
    /**
     * The destination address.
     */
    Ip4Addr dst_addr;
    
    /**
     * The TTL and protocol fields combined.
     */
    Ip4TtlProto ttl_proto;
    
    /**
     * The interface through which the packet was received.
     */
    IpIface<TheIpStack> *iface;
};

/**
 * Stores reusable data for sending multiple packets efficiently.
 * 
 * This structure is filled in by @ref IpStack::prepareSendIp4Dgram and can then be
 * used with @ref IpStack::sendIp4DgramFast multiple times to send datagrams.
 * 
 * Values filled in this structure are only valid temporarily because the
 * @ref route_info contains a pointer to an interface, which could be removed.
 * 
 * @tparam TheIpStack The @ref IpStack class type.
 */
template <typename TheIpStack>
struct IpSendPreparedIp4 {
    /**
     * Routing information (may be read externally if found useful).
     */
    IpRouteInfoIp4<TheIpStack> route_info;
    
    /**
     * Partially calculated IP header checksum (should not be used externally).
     */
    IpChksumAccumulator::State partial_chksum_state;
};

/** @} */

}

#endif
