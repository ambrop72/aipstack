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

#ifndef AIPSTACK_IPSTACK_TYPES_H
#define AIPSTACK_IPSTACK_TYPES_H

#include <cstdint>

#include <aipstack/misc/EnumBitfieldUtils.h>
#include <aipstack/misc/EnumUtils.h>
#include <aipstack/infra/Chksum.h>
#include <aipstack/proto/Ip4Proto.h>
#include <aipstack/proto/Icmp4Proto.h>
#include <aipstack/ip/IpAddr.h>
#include <aipstack/platform/PlatformFacade.h>

namespace AIpStack {

#ifndef IN_DOXYGEN
template <typename> class IpStack;
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
     * Default constructor for no IP address asignment.
     * 
     * Sets @ref present to false and other members to zero.
     */
    inline constexpr IpIfaceIp4AddrSetting () = default;

    /**
     * Constructor for a valid IP address assignment.
     * 
     * Sets @ref present to true and other members as specified.
     * 
     * @param prefix_ Subnet prefix length.
     * @param addr_ IPv4 address.
     */
    inline constexpr IpIfaceIp4AddrSetting (std::uint8_t prefix_, Ip4Addr addr_) :
        present(true),
        prefix(prefix_),
        addr(addr_)
    {}

    /**
     * Whether an IP address is or should be assigned.
     * 
     * If this is false, then other members of this structure are meaningless.
     */
    bool present = false;
    
    /**
     * The subnet prefix length.
     */
    std::uint8_t prefix = 0;
    
    /**
     * The IPv4 address.
     */
    Ip4Addr addr = Ip4Addr::ZeroAddr();
};

/**
 * Represents the IPv4 gateway configuration of a network interface.
 * 
 * Structures of this type are passed to @ref IpIface::setIp4Gateway
 * and returned by @ref IpIface::getIp4Gateway.
 */
struct IpIfaceIp4GatewaySetting {
    /**
     * Default constructor for no gateway asignment.
     * 
     * Sets @ref present to false and other members to zero.
     */
    inline constexpr IpIfaceIp4GatewaySetting () = default;

    /**
     * Constructor for a valid gateway assignment.
     * 
     * Sets @ref present to true and other members as specified.
     * 
     * @param addr_ Gateway address.
     */
    inline constexpr IpIfaceIp4GatewaySetting (Ip4Addr addr_) :
        present(true),
        addr(addr_)
    {}

    /**
     * Whether a gateway address is or should be assigned.
     * 
     * If this is false, then other members of this structure are meaningless.
     */
    bool present = false;
    
    /**
     * The gateway address.
     */
    Ip4Addr addr = Ip4Addr::ZeroAddr();
};

/**
 * Contains cached information about the IPv4 address configuration of a
 * network interface.
 * 
 * A pointer to a structure of this type can be obtained using
 * @ref IpDriverIface::getIp4Addrs. In addition to the IP address
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
    std::uint8_t prefix;
};

/**
 * Contains state reported by IP interface drivers to the IP stack.
 * 
 * Structures of this type are returned by @ref IpIface::getDriverState,
 * as well as by @ref IpIfaceDriverParams::get_state as part of the driver
 * interface.
 */
struct IpIfaceDriverState {
    /**
     * Whether the link is up.
     */
    bool link_up = true;
};

/**
 * Contains definitions of flags as accepted by @ref AIpStack::IpStack::sendIp4Dgram
 * "IpStack::sendIp4Dgram" and @ref AIpStack::IpStack::prepareSendIp4Dgram
 * "IpStack::prepareSendIp4Dgram".
 * 
 * Operators provided by @ref AIPSTACK_ENUM_BITFIELD_OPS are available.
 * 
 * Note that internally in the implementation, standard IP flags (as in the IP
 * header) are used with this enum type for performance reasons. To support this,
 * the @ref IpSendFlags::DontFragmentFlag "DontFragmentFlag" flag has the same
 * value as the IP flag "DF" and the other (non-IP) flags defined here use low bits
 * which do not conflict with IP flags.
 */
enum class IpSendFlags : std::uint16_t {
    /**
     * Allow broadcast.
     * 
     * This flag is required in order to send to a local broadcast or all-ones address.
     * If it is set then sending to non-broadcast addresses is still allowed.
     */
    AllowBroadcastFlag = std::uint16_t(1) << 0,

    /**
     * Allow sending from from a non-local address.
     * 
     * This flag is required in order to send using a source address that is not the
     * address of the outgoing network interface.
     */
    AllowNonLocalSrc = std::uint16_t(1) << 1,

    /**
     * Do-not-fragment flag.
     * 
     * Using this flag will both prevent fragmentation of the outgoing
     * datagram as well as set the Dont-Fragment flag in the IP header.
     */
    DontFragmentFlag = ToUnderlyingType(Ip4Flags::DF),

    /**
     * Mask of all flags which may be passed to send functions.
     */
    AllFlags = AllowBroadcastFlag|AllowNonLocalSrc|DontFragmentFlag,
};
#ifndef IN_DOXYGEN
AIPSTACK_ENUM_BITFIELD_OPS(IpSendFlags)
#endif

#ifndef IN_DOXYGEN

// Convert IP flags to IpSendFlags (for internal use).
inline constexpr IpSendFlags IpFlagsToSendFlags(Ip4Flags flags) {
    return IpSendFlags(ToUnderlyingType(flags));
}

// Extract only IP flags from IpSendFlags (for internal use).
inline constexpr Ip4Flags IpFlagsInSendFlags(IpSendFlags send_flags) {
    // One might argue we should AND with 0xE000 but this is fine as
    // well since we the defined flags in IpSendFlags use very low
    // low bits, and may be a bit more efficient.
    return Ip4Flags(ToUnderlyingType(send_flags) & 0xFF00);
}

#endif

/**
 * Contains information about a received ICMP Destination Unreachable message.
 */
struct Ip4DestUnreachMeta {
    /**
     * The ICMP code.
     * 
     * For example, `Icmp4Code::DestUnreachFragNeeded` may be of interest.
     */
    Icmp4Code icmp_code = Icmp4Code::Zero;
    
    /**
     * The "Rest of Header" part of the ICMP header (4 bytes).
     */
    Icmp4RestType icmp_rest = {};
};

/**
 * Encapsulates a pair of IPv4 TTL and protocol number.
 * 
 * An @ref Ip4TtlProto object is defined by its value which is a 16-bit unsigned
 * integer representing the TTL and protocol number in the same way as in the
 * IPv4 header. That is, the TTL is stored in the higher 8 bits and the protocol
 * in the lower 8 bits.
 */
class Ip4TtlProto {
private:
    std::uint16_t m_value;
    
public:
    /**
     * Default constructor, initializes the value to zero.
     */
    inline constexpr Ip4TtlProto () :
        m_value(0)
    {}
    
    /**
     * Constructor from a value (TTL and protocol number together).
     * 
     * @param value Value to initialize with.
     */
    inline constexpr Ip4TtlProto (std::uint16_t value) :
        m_value(value)
    {}
    
    /**
     * Constructor from TTL and protocol number.
     * 
     * @param ttl The TTL.
     * @param proto The protocol number.
     */
    inline constexpr Ip4TtlProto (std::uint8_t ttl, Ip4Protocol proto) :
        m_value(std::uint16_t((std::uint16_t(ttl) << 8) | ToUnderlyingType(proto)))
    {}

    /**
     * Return the value of this object (TTL and protocol number together).
     * 
     * @return The value.
     */
    inline constexpr std::uint16_t value () const {
        return m_value;
    }
    
    /**
     * Return the TTL.
     * 
     * @return The TTL (higher 8 bits of the value).
     */
    inline constexpr std::uint8_t ttl () const {
        return std::uint8_t(m_value >> 8);
    }
    
    /**
     * Return the protocol number.
     * 
     * @return The protocol number (lower 8 bits of the value).
     */
    inline constexpr Ip4Protocol proto () const {
        return Ip4Protocol(m_value & 0xFF);
    }
};

/**
 * Encapsulates parameters passed to protocol handler constructors.
 * 
 * See @ref IpProtocolHandlerStub::IpProtocolHandlerStub for the documentation
 * of protocol handler construction.
 * 
 * @tparam Arg Template parameter of @ref IpStack.
 */
template <typename Arg>
struct IpProtocolHandlerArgs {
    /**
     * The platform facade, as passed to the @ref IpStack::IpStack constructor.
     */
    PlatformFacade<typename Arg::PlatformImpl> platform;
    
    /**
     * A pointer to the IP stack.
     */
    IpStack<Arg> *stack;
};

/**
 * Encapsulates route information returned route functions.
 * 
 * Functions such as @ref IpStack::routeIp4 and @ref IpStack::routeIp4ForceIface will fill
 * in this structure. The result is only valid temporarily because it contains a pointer to
 * an interface, which could be removed.
 * 
 * @tparam Arg Template parameter of @ref IpStack.
 */
template <typename Arg>
struct IpRouteInfoIp4 {
    /**
     * The interface to send through.
     */
    IpIface<Arg> *iface;
    
    /**
     * The address of the next hop.
     */
    Ip4Addr addr;
};

/**
 * Encapsulates information about a received IPv4 datagram.
 * 
 * This is filled in by the stack and passed to the recvIp4Dgram function of
 * protocol handlers and also to @ref IpIfaceListener::Ip4DgramHandler.
 * 
 * @tparam Arg Template parameter of @ref IpStack.
 */
template <typename Arg>
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
    IpIface<Arg> *iface;

    /**
     * The length of the IPv4 header in bytes.
     */
    std::uint8_t header_len;
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
 * @tparam Arg Template parameter of @ref IpStack.
 */
template <typename Arg>
struct IpSendPreparedIp4 {
    /**
     * Routing information (may be read externally if found useful).
     */
    IpRouteInfoIp4<Arg> route_info;
    
    /**
     * Partially calculated IP header checksum (should not be used externally).
     */
    IpChksumAccumulator::State partial_chksum_state;
};

/** @} */

}

#endif
