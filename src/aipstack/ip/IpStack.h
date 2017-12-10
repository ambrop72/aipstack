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

#ifndef AIPSTACK_IPSTACK_H
#define AIPSTACK_IPSTACK_H

#include <stddef.h>
#include <stdint.h>

#include <aipstack/meta/ListForEach.h>
#include <aipstack/meta/TypeListUtils.h>
#include <aipstack/meta/FuncUtils.h>
#include <aipstack/meta/InstantiateVariadic.h>
#include <aipstack/misc/Assert.h>
#include <aipstack/misc/Use.h>
#include <aipstack/misc/Hints.h>
#include <aipstack/misc/MinMax.h>
#include <aipstack/misc/EnumBitfieldUtils.h>
#include <aipstack/misc/NonCopyable.h>
#include <aipstack/misc/ResourceTuple.h>
#include <aipstack/structure/LinkedList.h>
#include <aipstack/structure/LinkModel.h>
#include <aipstack/structure/StructureRaiiWrapper.h>
#include <aipstack/structure/Accessor.h>
#include <aipstack/infra/Err.h>
#include <aipstack/infra/Buf.h>
#include <aipstack/infra/Chksum.h>
#include <aipstack/infra/SendRetry.h>
#include <aipstack/infra/TxAllocHelper.h>
#include <aipstack/infra/Options.h>
#include <aipstack/infra/ObserverNotification.h>
#include <aipstack/infra/Instance.h>
#include <aipstack/proto/IpAddr.h>
#include <aipstack/proto/Ip4Proto.h>
#include <aipstack/proto/Icmp4Proto.h>
#include <aipstack/ip/IpStackHelperTypes.h>
#include <aipstack/ip/IpIface.h>
#include <aipstack/ip/IpIfaceListener.h>
#include <aipstack/ip/IpIfaceStateObserver.h>
#include <aipstack/platform/PlatformFacade.h>

namespace AIpStack {

/**
 * @defgroup ip-stack IP Network Layer
 * @brief Implements the network layer and integrates the operation of network
 * interfaces and transport protocols.
 * 
 * This module contains the @ref IpStack class which implements the IP network
 * layer and manages the configured IP protocol handlers (e.g. TCP, UDP).
 * 
 * This module also contains the service definitions for certain internal classes
 * used by @ref IpStack but which need to be configured by the application.
 * Specifically, the application needs to provide instantiated
 * @ref IpReassemblyService and @ref IpPathMtuCacheService.
 * 
 * Protocol handlers are configured statically by passing a compile-time list of
 * protocor-handler services to @ref IpStackService::Compose. The required API for
 * protocol handlers is documented as part of the @ref IpProtocolHandlerStub class.
 * 
 * Network interfaces are added by constructing instances of classes derived from
 * @ref IpIface. The @ref IpStack maintains a list of network interfaces
 * in order to implement IP functionality, but does not otherwise manage them.
 * 
 * Application code which uses different transport-layer protocols (such as TCP) is
 * expected to go through @ref IpStack to gain access to the appropriate protocol
 * handlers, using @ref IpStack::GetProtocolType and @ref IpStack::getProtocol.
 * 
 * @{
 */

/**
 * IPv4 network layer implementation.
 * 
 * This class provides basic IPv4 services. It communicates with interface
 * drivers on one end and with protocol handlers on the other.
 * 
 * Applications should configure and initialize this class and manage network
 * interface using the @ref Iface class. Actual network access should be done
 * using the APIs provided by specific protocol handlers, which are exposed
 * via @ref getProtocol.
 * 
 * @tparam Arg Instantiation parameters (instantiate via @ref IpStackService).
 */
template <typename Arg>
class IpStack :
    private NonCopyable<IpStack<Arg>>
{
    template <typename> friend class IpIface;
    template <typename> friend class IpIfaceListener;
    
    AIPSTACK_USE_TYPES(Arg, (Params, ProtocolServicesList))
    AIPSTACK_USE_VALS(Params, (HeaderBeforeIp, IcmpTTL, AllowBroadcastPing))
    AIPSTACK_USE_TYPES(Params, (PathMtuCacheService, ReassemblyService))

public:
    /**
     * The platform implementation type, as given to @ref IpStackService::Compose.
     */
    using PlatformImpl = typename Arg::PlatformImpl;
    
    /**
     * The platform facade type corresponding to @ref PlatformImpl.
     */
    using Platform = PlatformFacade<PlatformImpl>;
    
private:
    AIPSTACK_USE_TYPE(Platform, TimeType)
    
    AIPSTACK_MAKE_INSTANCE(Reassembly, (ReassemblyService::template Compose<PlatformImpl>))
    
    AIPSTACK_MAKE_INSTANCE(PathMtuCache, (
        PathMtuCacheService::template Compose<PlatformImpl, IpStack>))
    
    // Instantiate the protocols.
    template <int ProtocolIndex>
    struct ProtocolHelper {
        // Get the protocol service.
        using ProtocolService = TypeListGet<ProtocolServicesList, ProtocolIndex>;
        
        // Expose the protocol number for TypeListGetMapped (GetProtocolType).
        using IpProtocolNumber = typename ProtocolService::IpProtocolNumber;
        
        // Instantiate the protocol.
        AIPSTACK_MAKE_INSTANCE(Protocol, (
            ProtocolService::template Compose<PlatformImpl, IpStack>))
        
        // Helper function to get the pointer to the protocol.
        inline static Protocol * get (IpStack *stack)
        {
            return &stack->m_protocols.template get<ProtocolIndex>();
        }
    };
    using ProtocolHelpersList =
        IndexElemList<ProtocolServicesList, ProtocolHelper>;
    
    static int const NumProtocols = TypeListLength<ProtocolHelpersList>::Value;
    
    // Create a list of the instantiated protocols, for the tuple.
    template <typename Helper>
    using ProtocolForHelper = typename Helper::Protocol;
    using ProtocolsList = MapTypeList<
        ProtocolHelpersList, TemplateFunc<ProtocolForHelper>>;
    
    // Helper to extract IpProtocolNumber from a ProtocolHelper.
    template <typename Helper>
    using ProtocolNumberForHelper = typename Helper::IpProtocolNumber;
    
public:
    /**
     * Number of bytes which must be available in outgoing datagrams for headers.
     * 
     * Buffers passed to send functions such as @ref sendIp4Dgram and
     * @ref sendIp4DgramFast must have at least this much space available in the
     * first buffer node before the data. This space is used by the IP layer
     * implementation to write the IP header and by lower-level protocol
     * implementations such as Ethernet for their own headers.
     */
    static size_t const HeaderBeforeIp4Dgram = HeaderBeforeIp + Ip4Header::Size;
    
    /**
     * The @ref IpProtocolHandlerArgs structure type for this @ref IpStack, encapsulating
     * parameters passed to protocol handler constructors.
     */
    using ProtocolHandlerArgs = IpProtocolHandlerArgs<IpStack>;
    
    /**
     * The @ref IpIface class for this @ref IpStack, representing a network interface.
     */
    using Iface = IpIface<IpStack>;
    
    /**
     * The @ref IpIfaceListener class for this @ref IpStack, for receiving IP datagrams
     * from a specific network interface.
     */
    using IfaceListener = IpIfaceListener<IpStack>;
    
private:
    using IfaceLinkModel = PointerLinkModel<Iface>;
    using IfaceListenerLinkModel = PointerLinkModel<IfaceListener>;
    
public:
    /**
     * Minimum permitted MTU and PMTU.
     * 
     * RFC 791 requires that routers can pass through 68 byte packets, so enforcing
     * this larger value theoreticlaly violates the standard. We need this to
     * simplify the implementation of TCP, notably so that the TCP headers do not
     * need to be fragmented and the DF flag never needs to be turned off. Note that
     * Linux enforces a minimum of 552, this must be perfectly okay in practice.
     */
    static uint16_t const MinMTU = 256;
    
    /**
     * Construct the IP stack.
     * 
     * Construction of the stack includes construction of all configured protocol
     * handlers.
     * 
     * @param platform The platform facade. The Platform type is simply
     *        PlatformFacade\<PlatformImpl\> where PlatformImpl is as given
     *        to @ref IpStackService::Compose.
     */
    IpStack (Platform platform) :
        m_reassembly(platform),
        m_path_mtu_cache(platform, this),
        m_next_id(0),
        m_protocols(ResourceTupleInitSame(), ProtocolHandlerArgs{platform, this})
    {}
    
    /**
     * Destruct the IP stack.
     * 
     * There must be no remaining interfaces associated with this stack
     * when the IP stack is destructed. Additionally, specific protocol
     * handlers may have their own destruction preconditions.
     */
    ~IpStack ()
    {
        AIPSTACK_ASSERT(m_iface_list.isEmpty())
    }
    
    /**
     * Return the platform facade.
     * 
     * @return The platform facade.
     */
    inline Platform platform () const
    {
        return m_reassembly.platform();
    }
    
    /**
     * Get the protocol instance type for a protocol number.
     * 
     * @tparam ProtocolNumber The IP procol number to get the type for.
     *         It must be the number of one of the configured procotols.
     */
    template <uint8_t ProtocolNumber>
    using GetProtocolType =
    #ifdef IN_DOXYGEN
        implementation_hidden;
    #else
        typename TypeListGetMapped<
            ProtocolHelpersList,
            TemplateFunc<ProtocolNumberForHelper>,
            WrapValue<uint8_t, ProtocolNumber>
        >::Protocol;
    #endif
    
    /**
     * Get the pointer to a protocol instance given the protocol instance type.
     * 
     * @tparam Protocol The protocol instance type. It must be the
     *         instance type of one of the configured protocols.
     * @return Pointer to protocol instance.
     */
    template <typename Protocol>
    inline Protocol * getProtocol ()
    {
        static int const ProtocolIndex =
            TypeListIndex<ProtocolsList, Protocol>::Value;
        return &m_protocols.template get<ProtocolIndex>();
    }
    
public:
    /**
     * The @ref IpRouteInfoIp4 structure type for this @ref IpStack, encapsulating route
     * information.
     */
    using RouteInfoIp4 = IpRouteInfoIp4<IpStack>;

    /**
     * The @ref IpRxInfoIp4 structure type for this @ref IpStack, encapsulating information
     * about a received IPv4 datagram.
     */
    using RxInfoIp4 = IpRxInfoIp4<IpStack>;

    /**
     * Send an IPv4 datagram.
     * 
     * This is the primary send function intended to be used by protocol handlers.
     * 
     * This function internally uses @ref routeIp4 or @ref routeIp4ForceIface (depending
     * on whether iface is given) to determine the required routing information. If this
     * fails, the error @ref IpErr::NO_IP_ROUTE will be returned.
     * 
     * This function will perform IP fragmentation unless send_flags includes
     * @ref IpSendFlags::DontFragmentFlag. If fragmentation would be needed but this
     * flag is set, the error @ref IpErr::FRAG_NEEDED will be returned. If sending one
     * fragment fails, further fragments are not sent.
     * 
     * Each attempt to send a datagram will result in assignment of an identification
     * number, except when the function fails with @ref IpErr::NO_IP_ROUTE or
     * @ref IpErr::FRAG_NEEDED as noted above. Identification numbers are generated
     * sequentially and there is no attempt to track which numbers are in use.
     *
     * Sending to a local broadcast or all-ones address is only allowed if send_flags
     * includes @ref IpSendFlags::AllowBroadcastFlag. Otherwise sending will fail with
     * the error @ref IpErr::BCAST_REJECTED.
     * 
     * @param addrs Source and destination address.
     * @param ttl_proto The TTL and protocol fields combined.
     * @param dgram The data to be sent. There must be space available before the
     *              data for the IPv4 header and lower-layer headers (reserving
     *              @ref HeaderBeforeIp4Dgram will suffice). The tot_len of the data
     *              must not exceed 2^16-1.
     * @param iface If not null, force sending through this interface.
     * @param retryReq If not null, this may provide notification when to retry sending
     *                 after an unsuccessful attempt (notification is not guaranteed).
     * @param send_flags IP flags to send. All flags declared in
     *        @ref IpSendFlags::DontFragmentFlag are allowed (other bits must not be
     *        present).
     * @return Success or error code.
     */
    AIPSTACK_NO_INLINE
    IpErr sendIp4Dgram (Ip4Addrs const &addrs, Ip4TtlProto ttl_proto, IpBufRef dgram,
                        Iface *iface, IpSendRetryRequest *retryReq,
                        IpSendFlags send_flags)
    {
        AIPSTACK_ASSERT(dgram.tot_len <= TypeMax<uint16_t>())
        AIPSTACK_ASSERT(dgram.offset >= Ip4Header::Size)
        AIPSTACK_ASSERT((send_flags & ~IpSendFlags::AllFlags) == EnumZero)
        
        // Reveal IP header.
        IpBufRef pkt = dgram.revealHeaderMust(Ip4Header::Size);
        
        // Find an interface and address for output.
        RouteInfoIp4 route_info;
        bool route_ok;
        if (AIPSTACK_UNLIKELY(iface != nullptr)) {
            route_ok = routeIp4ForceIface(addrs.remote_addr, iface, route_info);
        } else {
            route_ok = routeIp4(addrs.remote_addr, route_info);
        }
        if (AIPSTACK_UNLIKELY(!route_ok)) {
            return IpErr::NO_IP_ROUTE;
        }
        
        // Check if sending is allowed (e.g. broadcast).
        IpErr check_err = checkSendIp4Allowed(addrs.remote_addr, send_flags,
                                              route_info.iface);
        if (AIPSTACK_UNLIKELY(check_err != IpErr::SUCCESS)) {
            return check_err;
        }

        // Check if fragmentation is needed...
        uint16_t pkt_send_len;
        
        if (AIPSTACK_UNLIKELY(pkt.tot_len > route_info.iface->getMtu())) {
            // Reject fragmentation?
            if (AIPSTACK_UNLIKELY((send_flags & IpSendFlags::DontFragmentFlag) != EnumZero)) {
                return IpErr::FRAG_NEEDED;
            }
            
            // Calculate length of first fragment.
            pkt_send_len = Ip4RoundFragLen(Ip4Header::Size, route_info.iface->getMtu());
            
            // Set the MoreFragments IP flag (will be cleared for the last fragment).
            send_flags |= IpSendFlags(Ip4FlagMF);
        } else {
            // First packet has all the data.
            pkt_send_len = pkt.tot_len;
        }
        
        // Write IP header fields and calculate header checksum inline...
        auto ip4_header = Ip4Header::MakeRef(pkt.getChunkPtr());
        IpChksumAccumulator chksum;
        
        uint16_t version_ihl_dscp_ecn = (uint16_t)((4 << Ip4VersionShift) | 5) << 8;
        chksum.addWord(WrapType<uint16_t>(), version_ihl_dscp_ecn);
        ip4_header.set(Ip4Header::VersionIhlDscpEcn(), version_ihl_dscp_ecn);
        
        chksum.addWord(WrapType<uint16_t>(), pkt_send_len);
        ip4_header.set(Ip4Header::TotalLen(), pkt_send_len);
        
        uint16_t ident = m_next_id++; // generate identification number
        chksum.addWord(WrapType<uint16_t>(), ident);
        ip4_header.set(Ip4Header::Ident(), ident);
        
        uint16_t flags_offset = (uint16_t)send_flags & IpOnlySendFlagsMask;
        chksum.addWord(WrapType<uint16_t>(), flags_offset);
        ip4_header.set(Ip4Header::FlagsOffset(), flags_offset);
        
        chksum.addWord(WrapType<uint16_t>(), ttl_proto.value);
        ip4_header.set(Ip4Header::TtlProto(), ttl_proto.value);
        
        chksum.addWords(&addrs.local_addr.data);
        ip4_header.set(Ip4Header::SrcAddr(), addrs.local_addr);
        
        chksum.addWords(&addrs.remote_addr.data);
        ip4_header.set(Ip4Header::DstAddr(), addrs.remote_addr);
        
        // Set the IP header checksum.
        ip4_header.set(Ip4Header::HeaderChksum(), chksum.getChksum());
        
        // Send the packet to the driver.
        // Fast path is no fragmentation, this permits tail call optimization.
        if (AIPSTACK_LIKELY((send_flags & IpSendFlags(Ip4FlagMF)) == EnumZero)) {
            return route_info.iface->driverSendIp4Packet(pkt, route_info.addr, retryReq);
        }
        
        // Slow path...
        return send_fragmented(pkt, route_info, send_flags, retryReq);
    }
    
private:
    IpErr send_fragmented (IpBufRef pkt, RouteInfoIp4 route_info,
                           IpSendFlags send_flags, IpSendRetryRequest *retryReq)
    {
        // Recalculate pkt_send_len (not passed for optimization).
        uint16_t pkt_send_len =
            Ip4RoundFragLen(Ip4Header::Size, route_info.iface->getMtu());
        
        // Send the first fragment.
        IpErr err = route_info.iface->driverSendIp4Packet(
            pkt.subTo(pkt_send_len), route_info.addr, retryReq);
        if (AIPSTACK_UNLIKELY(err != IpErr::SUCCESS)) {
            return err;
        }
        
        // Get back the dgram (we don't pass it for better optimization).
        IpBufRef dgram = pkt.hideHeader(Ip4Header::Size);
        
        // Calculate the next fragment offset and skip the sent data.
        uint16_t fragment_offset = pkt_send_len - Ip4Header::Size;
        dgram.skipBytes(fragment_offset);
        
        // Send remaining fragments.
        while (true) {
            // We must send fragments such that the fragment offset is a multiple of 8.
            // This is achieved by Ip4RoundFragLen.
            AIPSTACK_ASSERT(fragment_offset % 8 == 0)
            
            // If this is the last fragment, calculate its length and clear
            // the MoreFragments flag. Otherwise pkt_send_len is still correct
            // and MoreFragments still set.
            size_t rem_pkt_length = Ip4Header::Size + dgram.tot_len;
            if (rem_pkt_length <= route_info.iface->getMtu()) {
                pkt_send_len = rem_pkt_length;
                send_flags &= ~IpSendFlags(Ip4FlagMF);
            }
            
            auto ip4_header = Ip4Header::MakeRef(pkt.getChunkPtr());
            
            // Write the fragment-specific IP header fields.
            ip4_header.set(Ip4Header::TotalLen(), pkt_send_len);
            uint16_t flags_offset = ((uint16_t)send_flags & IpOnlySendFlagsMask) |
                                    (fragment_offset / 8);
            ip4_header.set(Ip4Header::FlagsOffset(), flags_offset);
            ip4_header.set(Ip4Header::HeaderChksum(), 0);
            
            // Calculate the IP header checksum.
            // Not inline since fragmentation is uncommon, better save program space.
            uint16_t calc_chksum = IpChksum(ip4_header.data, Ip4Header::Size);
            ip4_header.set(Ip4Header::HeaderChksum(), calc_chksum);
            
            // Construct a packet with header and partial data.
            IpBufNode data_node = dgram.toNode();
            IpBufNode header_node;
            IpBufRef frag_pkt = pkt.subHeaderToContinuedBy(
                Ip4Header::Size, &data_node, pkt_send_len, &header_node);
            
            // Send the packet to the driver.
            err = route_info.iface->driverSendIp4Packet(
                frag_pkt, route_info.addr, retryReq);
            
            // If this was the last fragment or there was an error, return.
            if ((send_flags & IpSendFlags(Ip4FlagMF)) == EnumZero ||
                AIPSTACK_UNLIKELY(err != IpErr::SUCCESS))
            {
                return err;
            }
            
            // Update the fragment offset and skip the sent data.
            uint16_t data_sent = pkt_send_len - Ip4Header::Size;
            fragment_offset += data_sent;
            dgram.skipBytes(data_sent);
        }
    }
    
public:
    /**
     * Stores reusable data for sending multiple packets efficiently.
     * 
     * This structure is filled in by @ref prepareSendIp4Dgram and can then be
     * used with @ref sendIp4DgramFast multiple times to send datagrams.
     * 
     * Values filled in this structure are only valid temporarily because the
     * route_info contains a pointer to an interface, which could be removed.
     */
    struct Ip4SendPrepared {
        /**
         * Routing information (may be read externally if found useful).
         */
        RouteInfoIp4 route_info;
        
        /**
         * Partially calculated IP header checksum (should not be used externally).
         */
        IpChksumAccumulator::State partial_chksum_state;
    };
    
    /**
     * Prepare for sending multiple datagrams with similar header fields.
     * 
     * This determines routing information, fills in common header fields and
     * stores internal information into the given @ref Ip4SendPrepared structure.
     * After this is successful, @ref sendIp4DgramFast can be used to send multiple
     * datagrams in succession, with IP header fields as specified here.
     * 
     * This mechanism is intended for bulk transmission where performance is desired.
     * Fragmentation or forcing an interface are not supported.
     * 
     * Sending to a local broadcast or all-ones address is only allowed if send_flags
     * includes @ref IpSendFlags::AllowBroadcastFlag. Otherwise sending will fail with
     * the error @ref IpErr::BCAST_REJECTED.
     * 
     * @param addrs Source and destination address.
     * @param ttl_proto The TTL and protocol fields combined.
     * @param header_end_ptr Pointer to the end of the IPv4 header (and start of data).
     *                       This must be the same location as for subsequent datagrams.
     * @param send_flags IP flags to send. All flags declared in
     *        @ref IpSendFlags::DontFragmentFlag are allowed (other bits must not be
     *        present).
     * @param prep Internal information is stored into this structure.
     * @return Success or error code.
     */
    AIPSTACK_ALWAYS_INLINE
    IpErr prepareSendIp4Dgram (Ip4Addrs const &addrs, Ip4TtlProto ttl_proto,
                               char *header_end_ptr, IpSendFlags send_flags,
                               Ip4SendPrepared &prep)
    {
        AIPSTACK_ASSERT((send_flags & ~IpSendFlags::AllFlags) == EnumZero)
        
        // Get routing information (fill in route_info).
        if (AIPSTACK_UNLIKELY(!routeIp4(addrs.remote_addr, prep.route_info))) {
            return IpErr::NO_IP_ROUTE;
        }
        
        // Check if sending is allowed (e.g. broadcast).
        IpErr check_err = checkSendIp4Allowed(addrs.remote_addr, send_flags,
                                              prep.route_info.iface);
        if (AIPSTACK_UNLIKELY(check_err != IpErr::SUCCESS)) {
            return check_err;
        }

        // Write IP header fields and calculate partial header checksum inline...
        auto ip4_header = Ip4Header::MakeRef(header_end_ptr - Ip4Header::Size);
        IpChksumAccumulator chksum;
        
        uint16_t version_ihl_dscp_ecn = (uint16_t)((4 << Ip4VersionShift) | 5) << 8;
        chksum.addWord(WrapType<uint16_t>(), version_ihl_dscp_ecn);
        ip4_header.set(Ip4Header::VersionIhlDscpEcn(), version_ihl_dscp_ecn);
        
        uint16_t flags_offset = (uint16_t)send_flags & IpOnlySendFlagsMask;
        chksum.addWord(WrapType<uint16_t>(), flags_offset);
        ip4_header.set(Ip4Header::FlagsOffset(), flags_offset);
        
        chksum.addWord(WrapType<uint16_t>(), ttl_proto.value);
        ip4_header.set(Ip4Header::TtlProto(), ttl_proto.value);
        
        chksum.addWords(&addrs.local_addr.data);
        ip4_header.set(Ip4Header::SrcAddr(), addrs.local_addr);
        
        chksum.addWords(&addrs.remote_addr.data);
        ip4_header.set(Ip4Header::DstAddr(), addrs.remote_addr);
        
        // Save the partial header checksum.
        prep.partial_chksum_state = chksum.getState();
        
        return IpErr::SUCCESS;
    }
    
    /**
     * Send a datagram after preparation with @ref prepareSendIp4Dgram.
     * 
     * This sends a single datagram with header fields as specified in a previous
     * @ref prepareSendIp4Dgram call.
     * 
     * This function does not support fragmentation. If the packet would be too
     * large, the error @ref IpErr::FRAG_NEEDED is returned.
     * 
     * @param prep Structure with internal information that was filled in
     *             using @ref prepareSendIp4Dgram. Note that such information is
     *             only valid temporarily (see the note in @ref Ip4SendPrepared).
     * @param dgram The data to be sent. There must be space available before the
     *              data for the IPv4 header and lower-layer headers (reserving
     *              @ref HeaderBeforeIp4Dgram will suffice), and this must be the
     *              same buffer that was used in @ref prepareSendIp4Dgram via the
     *              header_end_ptr argument. The tot_len of the data must not
     *              exceed 2^16-1.
     * @param retryReq If not null, this may provide notification when to retry sending
     *                 after an unsuccessful attempt (notification is not guaranteed).
     * @return Success or error code.
     */
    AIPSTACK_ALWAYS_INLINE
    IpErr sendIp4DgramFast (Ip4SendPrepared const &prep, IpBufRef dgram,
                            IpSendRetryRequest *retryReq)
    {
        AIPSTACK_ASSERT(dgram.tot_len <= TypeMax<uint16_t>())
        AIPSTACK_ASSERT(dgram.offset >= Ip4Header::Size)
        
        // Reveal IP header.
        IpBufRef pkt = dgram.revealHeaderMust(Ip4Header::Size);
        
        // This function does not support fragmentation.
        if (AIPSTACK_UNLIKELY(pkt.tot_len > prep.route_info.iface->getMtu())) {
            return IpErr::FRAG_NEEDED;
        }
        
        // Write remaining IP header fields and continue calculating header checksum...
        auto ip4_header = Ip4Header::MakeRef(pkt.getChunkPtr());
        IpChksumAccumulator chksum(prep.partial_chksum_state);
        
        chksum.addWord(WrapType<uint16_t>(), pkt.tot_len);
        ip4_header.set(Ip4Header::TotalLen(), pkt.tot_len);
        
        uint16_t ident = m_next_id++; // generate identification number
        chksum.addWord(WrapType<uint16_t>(), ident);
        ip4_header.set(Ip4Header::Ident(), ident);
        
        // Set the IP header checksum.
        ip4_header.set(Ip4Header::HeaderChksum(), chksum.getChksum());
        
        // Send the packet to the driver.
        return prep.route_info.iface->driverSendIp4Packet(
            pkt, prep.route_info.addr, retryReq);
    }

private:
    static uint16_t const IpOnlySendFlagsMask = 0xFF00;
    
    inline static IpErr checkSendIp4Allowed (Ip4Addr dst_addr, IpSendFlags send_flags, 
                                             Iface *iface)
    {
        if (AIPSTACK_LIKELY((send_flags & IpSendFlags::AllowBroadcastFlag) == EnumZero)) {
            if (AIPSTACK_UNLIKELY(dst_addr.isAllOnes())) {
                return IpErr::BCAST_REJECTED;
            }

            if (AIPSTACK_LIKELY(iface->m_have_addr)) {
                if (AIPSTACK_UNLIKELY(dst_addr == iface->m_addr.bcastaddr)) {
                    return IpErr::BCAST_REJECTED;
                }
            }
        }
        
        return IpErr::SUCCESS;
    }

public:
    /**
     * Determine routing for the given destination address.
     * 
     * Determines the interface and next hop address for sending a packet to
     * the given address. The logic is:
     * - If there is any interface with an address configured for which the
     *   destination address belongs to the subnet of the interface, the
     *   resulting interface is the most recently added interface out of
     *   such interfaces with the longest prefix length, and the resulting
     *   hop address is the destination address.
     * - Otherwise, if any interface has a gateway configured, the resulting
     *   interface is the most recently added such interface, and the
     *   resulting hop address is the gateway address of that interface.
     * - Otherwise, the function fails (returns false).
     * 
     * @param dst_addr Destination address to determine routing for.
     * @param route_info Routing information will be written here.
     * @return True on success (route_info was filled in),
     *         false on error (route_info was not changed).
     */
    bool routeIp4 (Ip4Addr dst_addr, RouteInfoIp4 &route_info)
    {
        int best_prefix = -1;
        Iface *best_iface = nullptr;
        
        for (Iface *iface = m_iface_list.first(); iface != nullptr;
             iface = m_iface_list.next(*iface))
        {
            if (iface->ip4AddrIsLocal(dst_addr)) {
                int iface_prefix = iface->m_addr.prefix;
                if (iface_prefix > best_prefix) {
                    best_prefix = iface_prefix;
                    best_iface = iface;
                }
            }
            else if (iface->m_have_gateway && best_iface == nullptr) {
                best_iface = iface;
            }
        }
        
        if (AIPSTACK_UNLIKELY(best_iface == nullptr)) {
            return false;
        }
        
        route_info.iface = best_iface;
        route_info.addr = (best_prefix >= 0) ? dst_addr : best_iface->m_gateway;
        
        return true;
    }
    
    /**
     * Determine routing for the given destination address through
     * the given interface.
     * 
     * This is like @ref routeIp4 restricted to one interface with the exception
     * that it also accepts the all-ones broadcast address. The logic is:
     * - If the destination address is all-ones, or the interface has an address
     *   configured and the destination address belongs to the subnet of the interface,
     *   the resulting hop address is the destination address (and the resulting
     *   interface is as given).
     * - Otherwise, if the interface has a gateway configured, the resulting
     *   hop address is the gateway address of the interface (and the resulting
     *   interface is as given).
     * - Otherwise, the function fails (returns false).
     * 
     * @param dst_addr Destination address to determine routing for.
     * @param iface Interface which is to be used.
     * @param route_info Routing information will be written here.
     * @return True on success (route_info was filled in),
     *         false on error (route_info was not changed).
     */
    bool routeIp4ForceIface (Ip4Addr dst_addr, Iface *iface, RouteInfoIp4 &route_info)
    {
        AIPSTACK_ASSERT(iface != nullptr)
        
        if (dst_addr.isAllOnes() || iface->ip4AddrIsLocal(dst_addr)) {
            route_info.addr = dst_addr;
        }
        else if (iface->m_have_gateway) {
            route_info.addr = iface->m_gateway;
        }
        else {
            return false;
        }
        route_info.iface = iface;
        return true;
    }
    
    /**
     * Handle an ICMP Packet Too Big message.
     * 
     * This function checks the Path MTU estimate for an address and lowers it
     * to min(interface_mtu, max(MinMTU, mtu_info)) if it is greater than that.
     * However, nothing is done if there is no existing Path MTU estimate for
     * the address. Also if there is no route for the address then the min is
     * not done.
     * 
     * If the Path MTU estimate was lowered, then all existing MtuRef setup
     * for this address are notified (@ref MtuRef::pmtuChanged are called),
     * directly from this function.
     * 
     * @param remote_addr Address to which the ICMP message applies.
     * @param mtu_info The next-hop-MTU from the ICMP message.
     * @return True if the Path MTU estimate was lowered, false if not.
     */
    inline bool handleIcmpPacketTooBig (Ip4Addr remote_addr, uint16_t mtu_info)
    {
        return m_path_mtu_cache.handlePacketTooBig(remote_addr, mtu_info);
    }
    
    /**
     * Ensure that a Path MTU estimate does not exceed the interface MTU.
     * 
     * This is like @ref handleIcmpPacketTooBig except that it only considers the
     * interface MTU. This should be called by a protocol handler when it is using
     * Path MTU Discovery and sending fails with the @ref IpErr::FRAG_NEEDED error.
     * The intent is to handle the case when the MTU of the interface through which
     * the address is routed has changed, because this is a local issue and would
     * not be detected via an ICMP message.
     * 
     * If the Path MTU estimate was lowered, then all existing MtuRef setup
     * for this address are notified (@ref MtuRef::pmtuChanged are called),
     * directly from this function.
     * 
     * @param remote_addr Address for which to check the Path MTU estimate.
     * @return True if the Path MTU estimate was lowered, false if not.
     */
    inline bool handleLocalPacketTooBig (Ip4Addr remote_addr)
    {
        return m_path_mtu_cache.handlePacketTooBig(remote_addr, TypeMax<uint16_t>());
    }
    
    /**
     * Check if the source address of a received datagram appears to be
     * a unicast address.
     * 
     * Specifically, it checks that the source address is not all-ones or a
     * multicast address (@ref Ip4Addr::isAllOnesOrMulticast) and that it
     * is not the local broadcast address of the interface from which the
     * datagram was received.
     * 
     * @param ip_info Information about the received datagram.
     * @return True if the source address appears to be a unicast address,
     *         false if not.
     */
    static bool checkUnicastSrcAddr (RxInfoIp4 const &ip_info)
    {
        return !ip_info.src_addr.isAllOnesOrMulticast() &&
               !ip_info.iface->ip4AddrIsLocalBcast(ip_info.src_addr);
    }
    
    /**
     * The @ref IpIfaceStateObserver class for this @ref IpStack, for observing changes in
     * the driver-reported state of a network interface.
     */
    using IfaceStateObserver = IpIfaceStateObserver<IpStack>;
    
private:
    using IfaceListenerList = LinkedList<
        MemberAccessor<IfaceListener, LinkedListNode<IfaceListenerLinkModel>,
                       &IfaceListener::m_list_node>,
        IfaceListenerLinkModel, false>;
    
    using IfaceList = LinkedList<
        MemberAccessor<Iface, LinkedListNode<IfaceLinkModel>, &Iface::m_iface_list_node>,
        IfaceLinkModel, false>;
    
    using BaseMtuRef = typename PathMtuCache::MtuRef;
    
public:
    /**
     * Allows keeping track of the Path MTU estimate for a remote address.
     */
    class MtuRef
    #ifndef IN_DOXYGEN
        :private BaseMtuRef
    #endif
    {
    public:
        /**
         * Construct the MTU reference.
         * 
         * The object is constructed in not-setup state, that is without an
         * associated remote address. To set the remote address, call @ref setup.
         * This function must be called before any other function in this
         * class is called.
         */
        MtuRef () = default;
        
        /**
         * Destruct the MTU reference, asserting not-setup state.
         * 
         * It is required to ensure the object is in not-setup state before
         * destructing it (by calling @ref reset if needed). The destructor
         * cannot do the reset itself because it does not have the @ref IpStack
         * pointer available (to avoid using additional memory).
         */
        ~MtuRef () = default;
        
        /**
         * Reset the MTU reference.
         * 
         * This resets the object to the not-setup state.
         *
         * NOTE: It is required to reset the object to not-setup state
         * before destructing it, if not already in not-setup state.
         * 
         * @param stack The IP stack.
         */
        inline void reset (IpStack *stack)
        {
            return BaseMtuRef::reset(mtu_cache(stack));
        }
        
        /**
         * Check if the MTU reference is in setup state.
         * 
         * @return True if in setup state, false if in not-setup state.
         */
        inline bool isSetup () const
        {
            return BaseMtuRef::isSetup();
        }
        
        /**
         * Setup the MTU reference for a specific remote address.
         * 
         * The object must be in not-setup state when this is called.
         * On success, the current PMTU estimate is provided and future PMTU
         * estimate changes will be reported via the @ref pmtuChanged callback.
         * 
         * WARNING: Do not destruct the object while it is in setup state.
         * First use @ref reset (or @ref moveFrom) to change the object to
         * not-setup state before destruction.
         * 
         * @param stack The IP stack.
         * @param remote_addr The remote address to observe the PMTU for.
         * @param iface NULL or the interface though which remote_addr would be
         *        routed, as an optimization.
         * @param out_pmtu On success, will be set to the current PMTU estimate
         *        (guaranteed to be at least MinMTU). On failure it will not be
         *        changed.
         * @return True on success (object enters setup state), false on failure
         *         (object remains in not-setup state).
         */
        inline bool setup (IpStack *stack, Ip4Addr remote_addr, Iface *iface,
                           uint16_t &out_pmtu)
        {
            return BaseMtuRef::setup(mtu_cache(stack), remote_addr, iface, out_pmtu);
        }

        /**
         * Move an MTU reference from another object to this one.
         * 
         * This object must be in not-setup state. Upon return, the 'src' object
         * will be in not-setup state and this object will be in whatever state
         * the 'src' object was. If the 'src' object was in setup state, this object
         * will be setup with the same remote address.
         * 
         * @param src The object to move from.
         */
        inline void moveFrom (MtuRef &src)
        {
            return BaseMtuRef::moveFrom(src);
        }
        
    protected:
        /**
         * Callback which reports changes of the PMTU estimate.
         * 
         * This is called whenever the PMTU estimate changes,
         * and only in setup state.
         * 
         * WARNING: Do not change this object in any way from this callback,
         * specifically do not call @ref reset or @ref moveFrom. Note that the
         * implementation calls all these callbacks for the same remote address
         * in a loop, and that the callbacks may be called from within
         * @ref handleIcmpPacketTooBig and @ref handleLocalPacketTooBig.
         * 
         * @param pmtu The new PMTU estimate (guaranteed to be at least MinMTU).
         */
        virtual void pmtuChanged (uint16_t pmtu) = 0;
        
    private:
        inline static PathMtuCache * mtu_cache (IpStack *stack)
        {
            return &stack->m_path_mtu_cache;
        }
    };
    
private:
    static void processRecvedIp4Packet (Iface *iface, IpBufRef pkt)
    {
        // Check base IP header length.
        if (AIPSTACK_UNLIKELY(!pkt.hasHeader(Ip4Header::Size))) {
            return;
        }
        
        // Get a reference to the IP header.
        auto ip4_header = Ip4Header::MakeRef(pkt.getChunkPtr());
        
        // We will be calculating the header checksum inline.
        IpChksumAccumulator chksum;
        
        // Read Version+IHL+DSCP+ECN and add to checksum.
        uint16_t version_ihl_dscp_ecn = ip4_header.get(Ip4Header::VersionIhlDscpEcn());
        chksum.addWord(WrapType<uint16_t>(), version_ihl_dscp_ecn);
        
        // Check IP version and header length...
        uint8_t version_ihl = version_ihl_dscp_ecn >> 8;
        uint8_t header_len;
        
        // Fast path is that the version is correctly 4 and the header
        // length is minimal (5 words = 20 bytes).
        if (AIPSTACK_LIKELY(version_ihl == ((4 << Ip4VersionShift) | 5))) {
            // Header length is minimal, no options. There is no need to check
            // pkt.hasHeader(header_len) since that was already done above.
            header_len = Ip4Header::Size;
        } else {
            // Check IP version.
            if (AIPSTACK_UNLIKELY((version_ihl >> Ip4VersionShift) != 4)) {
                return;
            }
            
            // Check header length.
            // We require the entire header to fit into the first buffer.
            header_len = (version_ihl & Ip4IhlMask) * 4;
            if (AIPSTACK_UNLIKELY(header_len < Ip4Header::Size ||
                               !pkt.hasHeader(header_len)))
            {
                return;
            }
            
            // Add options to checksum.
            chksum.addEvenBytes(ip4_header.data + Ip4Header::Size,
                                header_len - Ip4Header::Size);
        }
        
        // Read total length and add to checksum.
        uint16_t total_len = ip4_header.get(Ip4Header::TotalLen());
        chksum.addWord(WrapType<uint16_t>(), total_len);
        
        // Check total length.
        if (AIPSTACK_UNLIKELY(total_len < header_len || total_len > pkt.tot_len)) {
            return;
        }
        
        // Create a reference to the payload.
        IpBufRef dgram = pkt.hideHeader(header_len).subTo(total_len - header_len);
        
        // Add ident and header checksum to checksum.
        chksum.addWord(WrapType<uint16_t>(), ip4_header.get(Ip4Header::Ident()));
        chksum.addWord(WrapType<uint16_t>(),
                       ip4_header.get(Ip4Header::HeaderChksum()));
        
        // Read TTL+protocol and add to checksum.
        Ip4TtlProto ttl_proto = ip4_header.get(Ip4Header::TtlProto());
        chksum.addWord(WrapType<uint16_t>(), ttl_proto.value);
        
        // Read addresses and add to checksum
        Ip4Addr src_addr = ip4_header.get(Ip4Header::SrcAddr());
        chksum.addWords(&src_addr.data);
        Ip4Addr dst_addr = ip4_header.get(Ip4Header::DstAddr());
        chksum.addWords(&dst_addr.data);
        
        // Get flags+offset and add to checksum.
        uint16_t flags_offset = ip4_header.get(Ip4Header::FlagsOffset());
        chksum.addWord(WrapType<uint16_t>(), flags_offset);        
        
        // Verify IP header checksum.
        if (AIPSTACK_UNLIKELY(chksum.getChksum() != 0)) {
            return;
        }
        
        // Check if the more-fragments flag is set or the fragment offset is nonzero.
        if (AIPSTACK_UNLIKELY((flags_offset & (Ip4FlagMF|Ip4OffsetMask)) != 0)) {
            // Only accept fragmented packets which are unicasts to the
            // incoming interface address. This is to prevent filling up
            // our reassembly buffers with irrelevant packets. Note that
            // we don't check this for non-fragmented packets for
            // performance reasons, it generally up to protocol handlers.
            if (!iface->ip4AddrIsLocalAddr(dst_addr)) {
                return;
            }
            
            // Get the more-fragments flag and the fragment offset in bytes.
            bool more_fragments = (flags_offset & Ip4FlagMF) != 0;
            uint16_t fragment_offset = (flags_offset & Ip4OffsetMask) * 8;
            
            // Perform reassembly.
            if (!iface->m_stack->m_reassembly.reassembleIp4(
                ip4_header.get(Ip4Header::Ident()), src_addr, dst_addr,
                ttl_proto.proto(), ttl_proto.ttl(), more_fragments,
                fragment_offset, ip4_header.data, dgram))
            {
                return;
            }
            // Continue processing the reassembled datagram.
            // Note, dgram was modified pointing to the reassembled data.
        }
        
        // Do the real processing now that the datagram is complete and
        // sanity checked.
        recvIp4Dgram({src_addr, dst_addr, ttl_proto, iface}, dgram);
    }
    
    static void recvIp4Dgram (RxInfoIp4 ip_info, IpBufRef dgram)
    {
        uint8_t proto = ip_info.ttl_proto.proto();
        
        // Pass to interface listeners. If any listener accepts the
        // packet, inhibit further processing.
        for (IfaceListener *lis = ip_info.iface->m_listeners_list.first();
             lis != nullptr; lis = ip_info.iface->m_listeners_list.next(*lis))
        {
            if (lis->m_proto == proto) {
                if (AIPSTACK_UNLIKELY(lis->recvIp4Dgram(ip_info, dgram))) {
                    return;
                }
            }
        }
        
        // Handle using a protocol listener if existing.
        bool not_handled = ListForBreak<ProtocolHelpersList>(
            [&] AIPSTACK_TL(Helper,
        {
            if (proto == Helper::IpProtocolNumber::Value) {
                Helper::get(ip_info.iface->m_stack)->recvIp4Dgram(
                    static_cast<RxInfoIp4 const &>(ip_info),
                    static_cast<IpBufRef>(dgram));
                return false;
            }
            return true;
        }));
        
        // If the packet was handled by a protocol handler, we are done.
        if (!not_handled) {
            return;
        }
        
        // Handle ICMP packets.
        if (proto == Ip4ProtocolIcmp) {
            return recvIcmp4Dgram(ip_info, dgram);
        }
    }
    
    static void recvIcmp4Dgram (RxInfoIp4 const &ip_info, IpBufRef const &dgram)
    {
        // Sanity check source address - reject broadcast addresses.
        if (AIPSTACK_UNLIKELY(!checkUnicastSrcAddr(ip_info))) {
            return;
        }
        
        // Check destination address.
        // Accept only: all-ones broadcast, subnet broadcast, interface address.
        bool is_broadcast_dst;
        if (AIPSTACK_LIKELY(ip_info.iface->ip4AddrIsLocalAddr(ip_info.dst_addr))) {
            is_broadcast_dst = false;
        } else {
            if (AIPSTACK_UNLIKELY(
                !ip_info.iface->ip4AddrIsLocalBcast(ip_info.dst_addr) &&
                ip_info.dst_addr != Ip4Addr::AllOnesAddr()))
            {
                return;
            }
            is_broadcast_dst = true;
        }
        
        // Check ICMP header length.
        if (AIPSTACK_UNLIKELY(!dgram.hasHeader(Icmp4Header::Size))) {
            return;
        }
        
        // Read ICMP header fields.
        auto icmp4_header = Icmp4Header::MakeRef(dgram.getChunkPtr());
        uint8_t type       = icmp4_header.get(Icmp4Header::Type());
        uint8_t code       = icmp4_header.get(Icmp4Header::Code());
        Icmp4RestType rest = icmp4_header.get(Icmp4Header::Rest());
        
        // Verify ICMP checksum.
        uint16_t calc_chksum = IpChksum(dgram);
        if (AIPSTACK_UNLIKELY(calc_chksum != 0)) {
            return;
        }
        
        // Get ICMP data by hiding the ICMP header.
        IpBufRef icmp_data = dgram.hideHeader(Icmp4Header::Size);
        
        IpStack *stack = ip_info.iface->m_stack;
        
        if (type == Icmp4TypeEchoRequest) {
            // Got echo request, send echo reply.
            // But if this is a broadcast request, respond only if allowed.
            if (is_broadcast_dst && !AllowBroadcastPing) {
                return;
            }
            stack->sendIcmp4EchoReply(rest, icmp_data, ip_info.src_addr, ip_info.iface);
        }
        else if (type == Icmp4TypeDestUnreach) {
            stack->handleIcmp4DestUnreach(code, rest, icmp_data, ip_info.iface);
        }
    }
    
    void sendIcmp4EchoReply (
        Icmp4RestType rest, IpBufRef data, Ip4Addr dst_addr, Iface *iface)
    {
        // Can only reply when we have an address assigned.
        if (AIPSTACK_UNLIKELY(!iface->m_have_addr)) {
            return;
        }
        
        // Allocate memory for headers.
        TxAllocHelper<Icmp4Header::Size, HeaderBeforeIp4Dgram>
            dgram_alloc(Icmp4Header::Size);
        
        // Write the ICMP header.
        auto icmp4_header = Icmp4Header::MakeRef(dgram_alloc.getPtr());
        icmp4_header.set(Icmp4Header::Type(),   Icmp4TypeEchoReply);
        icmp4_header.set(Icmp4Header::Code(),   0);
        icmp4_header.set(Icmp4Header::Chksum(), 0);
        icmp4_header.set(Icmp4Header::Rest(),   rest);
        
        // Construct the datagram reference with header and data.
        IpBufNode data_node = data.toNode();
        dgram_alloc.setNext(&data_node, data.tot_len);
        IpBufRef dgram = dgram_alloc.getBufRef();
        
        // Calculate ICMP checksum.
        uint16_t calc_chksum = IpChksum(dgram);
        icmp4_header.set(Icmp4Header::Chksum(), calc_chksum);
        
        // Send the datagram.
        Ip4Addrs addrs = {iface->m_addr.addr, dst_addr};
        sendIp4Dgram(addrs, {IcmpTTL, Ip4ProtocolIcmp}, dgram, iface, nullptr,
                     IpSendFlags());
    }
    
    void handleIcmp4DestUnreach (
        uint8_t code, Icmp4RestType rest, IpBufRef icmp_data, Iface *iface)
    {
        // Check base IP header length.
        if (AIPSTACK_UNLIKELY(!icmp_data.hasHeader(Ip4Header::Size))) {
            return;
        }
        
        // Read IP header fields.
        auto ip4_header = Ip4Header::MakeRef(icmp_data.getChunkPtr());
        uint8_t version_ihl    = ip4_header.get(Ip4Header::VersionIhlDscpEcn()) >> 8;
        uint16_t total_len     = ip4_header.get(Ip4Header::TotalLen());
        uint16_t ttl_proto     = ip4_header.get(Ip4Header::TtlProto());
        Ip4Addr src_addr       = ip4_header.get(Ip4Header::SrcAddr());
        Ip4Addr dst_addr       = ip4_header.get(Ip4Header::DstAddr());
        
        // Check IP version.
        if (AIPSTACK_UNLIKELY((version_ihl >> Ip4VersionShift) != 4)) {
            return;
        }
        
        // Check header length.
        // We require the entire header to fit into the first buffer.
        uint8_t header_len = (version_ihl & Ip4IhlMask) * 4;
        if (AIPSTACK_UNLIKELY(header_len < Ip4Header::Size ||
                           !icmp_data.hasHeader(header_len)))
        {
            return;
        }
        
        // Check that total_len includes at least the header.
        if (AIPSTACK_UNLIKELY(total_len < header_len)) {
            return;
        }
        
        // Create the Ip4DestUnreachMeta struct.
        Ip4DestUnreachMeta du_meta = {code, rest};
        
        // Create the RxInfoIp4 struct.
        RxInfoIp4 ip_info = {src_addr, dst_addr, ttl_proto, iface};
        
        // Get the included IP data.
        size_t data_len = MinValueU(icmp_data.tot_len, total_len) - header_len;
        IpBufRef dgram_initial = icmp_data.hideHeader(header_len).subTo(data_len);
        
        // Dispatch based on the protocol.
        ListForBreak<ProtocolHelpersList>([&] AIPSTACK_TL(Helper, {
            if (ip_info.ttl_proto.proto() == Helper::IpProtocolNumber::Value) {
                Helper::get(this)->handleIp4DestUnreach(
                    static_cast<Ip4DestUnreachMeta const &>(du_meta),
                    static_cast<RxInfoIp4 const &>(ip_info),
                    static_cast<IpBufRef>(dgram_initial));
                return false;
            }
            return true;
        }));
    }
    
private:
    Reassembly m_reassembly;
    PathMtuCache m_path_mtu_cache;
    StructureRaiiWrapper<IfaceList> m_iface_list;
    uint16_t m_next_id;
    InstantiateVariadic<ResourceTuple, ProtocolsList> m_protocols;
};


/**
 * Static configuration options for @ref IpStackService.
 */
struct IpStackOptions {
    /**
     * Required space for headers before the IP header in outgoing packets.
     * 
     * This should be the maximum of the required space of any IP interface
     * driver that may be used.
     */
    AIPSTACK_OPTION_DECL_VALUE(HeaderBeforeIp, size_t, 14)
    
    /**
     * TTL of outgoing ICMP packets.
     */
    AIPSTACK_OPTION_DECL_VALUE(IcmpTTL, uint8_t, 64)
    
    /**
     * Whether to respond to broadcast pings.
     * 
     * Broadcast pings are those with an all-ones or local-broadcast destination
     * address.
     */
    AIPSTACK_OPTION_DECL_VALUE(AllowBroadcastPing, bool, false)
    
    /**
     * Path MTU Discovery parameters/implementation.
     * 
     * This must be @ref IpPathMtuCacheService instantiated with the desired options.
     */
    AIPSTACK_OPTION_DECL_TYPE(PathMtuCacheService, void)
    
    /**
     * IP Reassembly parameters/implementation.
     * 
     * This must be @ref IpReassemblyService instantiated with the desired options.
     */
    AIPSTACK_OPTION_DECL_TYPE(ReassemblyService, void)
};

/**
 * Service definition for @ref IpStack.
 * 
 * The template parameters of this class are assignments of options defined in
 * @ref IpStackOptions, for example: AIpStack::IpStackOptions::IcmpTTL::Is\<16\>.
 * 
 * To to obtain an @ref IpStack class type, use @ref AIPSTACK_MAKE_INSTANCE with
 * @ref Compose, like this:
 * 
 * ```
 * using MyIpStackService = AIpStack::IpStackService<...>;
 * AIPSTACK_MAKE_INSTANCE(MyIpStack, (MyIpStackService::template Compose<
 *     PlatformImpl, ProtocolServicesList>))
 * MyIpStack ip_stack(...);
 * ```
 * 
 * @tparam Options Assignments of options defined in @ref IpStackOptions.
 */
template <typename... Options>
class IpStackService {
    template <typename>
    friend class IpStack;
    
    AIPSTACK_OPTION_CONFIG_VALUE(IpStackOptions, HeaderBeforeIp)
    AIPSTACK_OPTION_CONFIG_VALUE(IpStackOptions, IcmpTTL)
    AIPSTACK_OPTION_CONFIG_VALUE(IpStackOptions, AllowBroadcastPing)
    AIPSTACK_OPTION_CONFIG_TYPE(IpStackOptions, PathMtuCacheService)
    AIPSTACK_OPTION_CONFIG_TYPE(IpStackOptions, ReassemblyService)
    
public:
    /**
     * Template for use with @ref AIPSTACK_MAKE_INSTANCE to get an @ref IpStack type.
     * 
     * See @ref IpStackService for an example of instantiating the @ref IpStack.
     * 
     * @tparam PlatformImpl_ Platform layer implementation, that is the PlatformImpl
     *         type to be used with @ref PlatformFacade.
     * @tparam ProtocolServicesList_ List of IP protocol handler services.
     *         For example, to support only TCP, use
     *         MakeTypeList\<IpTcpProtoService\<...\>\> with appropriate
     *         parameters passed to IpTcpProtoService.
     */
    template <typename PlatformImpl_, typename ProtocolServicesList_>
    struct Compose {
#ifndef IN_DOXYGEN
        using PlatformImpl = PlatformImpl_;
        using ProtocolServicesList = ProtocolServicesList_;
        using Params = IpStackService;
        AIPSTACK_DEF_INSTANCE(Compose, IpStack)        
#endif
    };
};

/** @} */

}

#endif
