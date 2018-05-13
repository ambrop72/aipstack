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

#include <cstddef>
#include <cstdint>

#include <aipstack/meta/ListForEach.h>
#include <aipstack/meta/TypeListUtils.h>
#include <aipstack/meta/FuncUtils.h>
#include <aipstack/meta/InstantiateVariadic.h>
#include <aipstack/meta/BasicMetaUtils.h>
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
#include <aipstack/proto/Ip4Proto.h>
#include <aipstack/proto/Icmp4Proto.h>
#include <aipstack/ip/IpAddr.h>
#include <aipstack/ip/IpStackTypes.h>
#include <aipstack/ip/IpIface.h>
#include <aipstack/ip/IpIfaceListener.h>
#include <aipstack/ip/IpIfaceStateObserver.h>
#include <aipstack/ip/IpDriverIface.h>
#include <aipstack/ip/IpMtuRef.h>
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
 * The @ref IpDriverIface class represents a logical network interface from the
 * perspective of an IP-level interface driver and acts as a gateway for communication
 * between the driver and the stack. Network interfaces are added/removed by
 * constructing/destructing instances of @ref IpDriverIface.
 * 
 * The @ref IpIface class represents a network interface in a more general context,
 * especially for the purpose of address configuration. An @ref IpIface instance
 * always exists as part of an @ref IpDriverIface instance; @ref IpDriverIface::iface
 * is used to retrieve the @ref IpIface. Interface drivers are expected to hide the
 * @ref IpDriverIface while exposing the @ref IpIface to allow the interface to be
 * configured externally.
 * 
 * The @ref IpStack maintains a list of network interfaces in order to implement IP
 * functionality, but does not otherwise manage them. Currently, there is no
 * facility for the stack to notify the application of interface addition, removal
 * or configuration changes, but it may be added in the future.
 * 
 * Application code which uses different transport-layer protocols (such as TCP) is
 * expected to go through @ref IpStack to gain access to the appropriate protocol
 * handlers, using @ref IpStack::GetProtoArg and @ref IpStack::getProtoApi.
 * 
 * @{
 */

/**
 * IPv4 network layer implementation.
 * 
 * This class provides basic IPv4 services. It communicates with interface
 * drivers on one end and with protocol handlers on the other.
 * 
 * Applications should configure and initialize this class in order to use any
 * IP services. Network interfaces are managed via @ref IpDriverIface and respective
 * @ref IpIface instances (see the @ref ip-stack module description for details).
 * 
 * Actual network access should be done using the APIs provided by specific protocol
 * handlers, which are exposed via @ref GetProtoArg and @ref getProtoApi; consult
 * the documentation of specific protocol implementations.
 * 
 * @tparam Arg An instantiation of the @ref IpStackService::Compose template or a
 *         dummy class derived from such; see @ref IpStackService for an example.
 */
template <typename Arg>
class IpStack :
    private NonCopyable<IpStack<Arg>>
{
    template <typename> friend class IpIface;
    template <typename> friend class IpIfaceListener;
    template <typename> friend class IpDriverIface;
    template <typename> friend class IpMtuRef;
    
    AIPSTACK_USE_TYPES(Arg, (Params, ProtocolServicesList))
    AIPSTACK_USE_VALS(Params, (HeaderBeforeIp, IcmpTTL, AllowBroadcastPing))
    AIPSTACK_USE_TYPES(Params, (PathMtuCacheService, ReassemblyService))

public:
    /**
     * The platform implementation type, as given to @ref AIpStack::IpStackService::Compose
     * "IpStackService::Compose".
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
        PathMtuCacheService::template Compose<PlatformImpl, Arg>))
    
    // Instantiate the protocols.
    template <int ProtocolIndex>
    struct ProtocolHelper {
        // Get the protocol service.
        using ProtocolService = TypeListGet<ProtocolServicesList, ProtocolIndex>;
        
        // Expose the protocol number.
        using IpProtocolNumber = typename ProtocolService::IpProtocolNumber;
        
        // Instantiate the protocol.
        AIPSTACK_MAKE_INSTANCE(Protocol, (
            ProtocolService::template Compose<PlatformImpl, Arg>))
        
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
    
    // This metaprogramming is for GetProtoArg and getProtoApi. It finds the protocol
    // handler whose getApi() function returns a reference to ProtoApi<Arg> for some type
    // Arg.
    template <template <typename> class ProtoApi>
    class GetProtoApiHelper {
        template <typename, typename = void>
        struct MatchApi {
            static bool const Matches = false;
        };

        template <typename ProtoApiArg, typename Protocol, typename Dummy>
        struct MatchApi<ProtoApi<ProtoApiArg> & (Protocol::*) (), Dummy> {
            static bool const Matches = true;
            using MatchArg = ProtoApiArg;
            using MatchProtocol = Protocol;
        };

        template <typename Helper>
        using MatchHelperApi = MatchApi<decltype(&Helper::Protocol::getApi)>;

        using MatchApiList = MapTypeList<ProtocolHelpersList, TemplateFunc<MatchHelperApi>>;

        template <typename Match>
        using CheckMatch = WrapBool<Match::Matches>;

        using MatchFindRes = TypeListFindMapped<
            MatchApiList, TemplateFunc<CheckMatch>, WrapBool<true>>;
        
        static_assert(MatchFindRes::Found, "Requested IP protocol not configured.");

        using TheMatch = TypeListGet<MatchApiList, MatchFindRes::Result::Value>;
        
    public:
        using ProtoArg = typename TheMatch::MatchArg;
        using Protocol = typename TheMatch::MatchProtocol;
    };

    using Iface = IpIface<Arg>;
    using IfaceListener = IpIfaceListener<Arg>;
    
    using IfaceLinkModel = PointerLinkModel<Iface>;
    using IfaceListenerLinkModel = PointerLinkModel<IfaceListener>;
    
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
    static std::size_t const HeaderBeforeIp4Dgram = HeaderBeforeIp + Ip4Header::Size;
    
    /**
     * Minimum permitted MTU and PMTU.
     * 
     * RFC 791 requires that routers can pass through 68 byte packets, so enforcing
     * this larger value theoreticlaly violates the standard. We need this to
     * simplify the implementation of TCP, notably so that the TCP headers do not
     * need to be fragmented and the DF flag never needs to be turned off. Note that
     * Linux enforces a minimum of 552, this must be perfectly okay in practice.
     */
    static std::uint16_t const MinMTU = 256;
    
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
    IpStack (PlatformFacade<PlatformImpl> platform) :
        m_reassembly(platform),
        m_path_mtu_cache(platform, this),
        m_next_id(0),
        m_protocols(ResourceTupleInitSame(), IpProtocolHandlerArgs<Arg>{platform, this})
    {}
    
    /**
     * Destruct the IP stack.
     * 
     * There must be no remaining network interfaces associated with this stack
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
    inline PlatformFacade<PlatformImpl> platform () const
    {
        return m_reassembly.platform();
    }

    /**
     * Get the template parameter for a protocol API class template.
     * 
     * This alias template searches for an IP protocol handler which provides the protocol
     * API specified by the `ProtoApi` template parameter (a class template) and returns
     * the template parameter for the specified protocol API template. It is an error if no
     * protocol handler configured for this @ref IpStack provides the requested API.
     * 
     * The @ref getProtoApi function returns an actual reference to the protocol API.
     * 
     * @tparam ProtoApi Class template which represents the protocol API, for example @ref
     *         UdpApi or @ref TcpApi.
     */
    template <template <typename> class ProtoApi>
    using GetProtoArg = typename GetProtoApiHelper<ProtoApi>::ProtoArg;

    /**
     * Get the reference to a protocol API given a protocol API class template.
     * 
     * It is an error if no protocol handler configured for this @ref IpStack provides the
     * requested API.
     * 
     * See also @ref GetProtoArg.
     * 
     * @tparam ProtoApi Class template which represents the protocol API, for example @ref
     *         UdpApi or @ref TcpApi.
     * @return Reference to protocol API.
     */
    template <template <typename> class ProtoApi>
    inline ProtoApi<GetProtoArg<ProtoApi>> & getProtoApi ()
    {
        using Protocol = typename GetProtoApiHelper<ProtoApi>::Protocol;
        static int const ProtocolIndex = TypeListIndex<ProtocolsList, Protocol>::Value;
        return m_protocols.template get<ProtocolIndex>().getApi();
    }

public:
    /**
     * Send an IPv4 datagram.
     * 
     * This is the primary send function intended to be used by protocol handlers.
     * 
     * This function internally uses @ref routeIp4 or @ref routeIp4ForceIface (depending
     * on whether iface is given) to determine the required routing information. If this
     * fails, the error @ref IpErr::NO_IP_ROUTE will be returned.
     * 
     * This function will perform IP fragmentation unless `send_flags` includes
     * @ref IpSendFlags::DontFragmentFlag. If fragmentation would be needed but this
     * flag is set, the error @ref IpErr::FRAG_NEEDED will be returned. If sending one
     * fragment fails, further fragments are not sent.
     * 
     * Each attempt to send a datagram will result in assignment of an identification
     * number, except when the function fails with @ref IpErr::NO_IP_ROUTE or
     * @ref IpErr::FRAG_NEEDED as noted above. Identification numbers are generated
     * sequentially and there is no attempt to track which numbers are in use.
     *
     * Sending to a local broadcast or all-ones address is only allowed if `send_flags`
     * includes @ref IpSendFlags::AllowBroadcastFlag. Otherwise sending will fail with
     * the error @ref IpErr::BCAST_REJECTED.
     * 
     * Sending using a source address which is not the address of the outgoing network
     * interface is only allowed if `send_flags `includes @ref IpSendFlags::AllowNonLocalSrc.
     * Note that that the presence of this fiag does not influence routing.
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
     * @param send_flags IP flags to send. All flags declared in @ref IpSendFlags are
     *        allowed (other bits must not be present).
     * @return Success or error code.
     */
    AIPSTACK_NO_INLINE
    IpErr sendIp4Dgram (Ip4AddrPair const &addrs, Ip4TtlProto ttl_proto, IpBufRef dgram,
                        IpIface<Arg> *iface, IpSendRetryRequest *retryReq,
                        IpSendFlags send_flags)
    {
        AIPSTACK_ASSERT(dgram.tot_len <= TypeMax<std::uint16_t>())
        AIPSTACK_ASSERT(dgram.offset >= Ip4Header::Size)
        AIPSTACK_ASSERT((send_flags & ~IpSendFlags::AllFlags) == EnumZero)
        
        // Reveal IP header.
        IpBufRef pkt = dgram.revealHeaderMust(Ip4Header::Size);
        
        // Find an interface and address for output.
        IpRouteInfoIp4<Arg> route_info;
        bool route_ok;
        if (AIPSTACK_UNLIKELY(iface != nullptr)) {
            route_ok = routeIp4ForceIface(addrs.remote_addr, iface, route_info);
        } else {
            route_ok = routeIp4(addrs.remote_addr, route_info);
        }
        if (AIPSTACK_UNLIKELY(!route_ok)) {
            return IpErr::NO_IP_ROUTE;
        }
        
        // Check if sending is allowed.
        IpErr check_err = checkSendIp4Allowed(addrs, send_flags, route_info.iface);
        if (AIPSTACK_UNLIKELY(check_err != IpErr::SUCCESS)) {
            return check_err;
        }

        // Check if fragmentation is needed...
        std::uint16_t pkt_send_len;
        
        if (AIPSTACK_UNLIKELY(pkt.tot_len > route_info.iface->getMtu())) {
            // Reject fragmentation?
            if (AIPSTACK_UNLIKELY((send_flags & IpSendFlags::DontFragmentFlag) != EnumZero)) {
                return IpErr::FRAG_NEEDED;
            }
            
            // Calculate length of first fragment.
            pkt_send_len = Ip4RoundFragLen(Ip4Header::Size, route_info.iface->getMtu());
            
            // Set the MoreFragments IP flag (will be cleared for the last fragment).
            send_flags |= IpFlagsToSendFlags(Ip4Flags::MF);
        } else {
            // First packet has all the data.
            pkt_send_len = std::uint16_t(pkt.tot_len);
        }
        
        // Write IP header fields and calculate header checksum inline...
        auto ip4_header = Ip4Header::MakeRef(pkt.getChunkPtr());
        IpChksumAccumulator chksum;
        
        std::uint16_t version_ihl_dscp_ecn = std::uint16_t((4 << Ip4VersionShift) | 5) << 8;
        chksum.addWord(WrapType<std::uint16_t>(), version_ihl_dscp_ecn);
        ip4_header.set(Ip4Header::VersionIhlDscpEcn(), version_ihl_dscp_ecn);
        
        chksum.addWord(WrapType<std::uint16_t>(), pkt_send_len);
        ip4_header.set(Ip4Header::TotalLen(), pkt_send_len);
        
        std::uint16_t ident = m_next_id++; // generate identification number
        chksum.addWord(WrapType<std::uint16_t>(), ident);
        ip4_header.set(Ip4Header::Ident(), ident);
        
        Ip4Flags flags_offset = IpFlagsInSendFlags(send_flags);
        chksum.addWord(WrapType<std::uint16_t>(), ToUnderlyingType(flags_offset));
        ip4_header.set(Ip4Header::FlagsOffset(), flags_offset);
        
        chksum.addWord(WrapType<std::uint16_t>(), ttl_proto.value());
        ip4_header.set(Ip4Header::TtlProto(), ttl_proto.value());
        
        chksum.addWord(WrapType<std::uint32_t>(), addrs.local_addr.value());
        ip4_header.set(Ip4Header::SrcAddr(), addrs.local_addr);
        
        chksum.addWord(WrapType<std::uint32_t>(), addrs.remote_addr.value());
        ip4_header.set(Ip4Header::DstAddr(), addrs.remote_addr);
        
        // Set the IP header checksum.
        ip4_header.set(Ip4Header::HeaderChksum(), chksum.getChksum());
        
        // Send the packet to the driver.
        // Fast path is no fragmentation, this permits tail call optimization.
        if (AIPSTACK_LIKELY((send_flags & IpFlagsToSendFlags(Ip4Flags::MF)) == EnumZero)) {
            return route_info.iface->m_params.send_ip4_packet(
                pkt, route_info.addr, retryReq);
        }
        
        // Slow path...
        return send_fragmented(pkt, route_info, send_flags, retryReq);
    }
    
private:
    IpErr send_fragmented (IpBufRef pkt, IpRouteInfoIp4<Arg> route_info,
                           IpSendFlags send_flags, IpSendRetryRequest *retryReq)
    {
        // Recalculate pkt_send_len (not passed for optimization).
        std::uint16_t pkt_send_len =
            Ip4RoundFragLen(Ip4Header::Size, route_info.iface->getMtu());
        
        // Send the first fragment.
        IpErr err = route_info.iface->m_params.send_ip4_packet(
            pkt.subTo(pkt_send_len), route_info.addr, retryReq);
        if (AIPSTACK_UNLIKELY(err != IpErr::SUCCESS)) {
            return err;
        }
        
        // Get back the dgram (we don't pass it for better optimization).
        IpBufRef dgram = pkt.hideHeader(Ip4Header::Size);
        
        // Calculate the next fragment offset and skip the sent data.
        std::uint16_t fragment_offset = pkt_send_len - Ip4Header::Size;
        dgram.skipBytes(fragment_offset);
        
        // Send remaining fragments.
        while (true) {
            // We must send fragments such that the fragment offset is a multiple of 8.
            // This is achieved by Ip4RoundFragLen.
            AIPSTACK_ASSERT(fragment_offset % 8 == 0)
            
            // If this is the last fragment, calculate its length and clear
            // the MoreFragments flag. Otherwise pkt_send_len is still correct
            // and MoreFragments still set.
            std::size_t rem_pkt_length = Ip4Header::Size + dgram.tot_len;
            if (rem_pkt_length <= route_info.iface->getMtu()) {
                pkt_send_len = std::uint16_t(rem_pkt_length);
                send_flags &= ~IpFlagsToSendFlags(Ip4Flags::MF);
            }
            
            auto ip4_header = Ip4Header::MakeRef(pkt.getChunkPtr());
            
            // Write the fragment-specific IP header fields.
            ip4_header.set(Ip4Header::TotalLen(), pkt_send_len);
            Ip4Flags flags_offset =
                IpFlagsInSendFlags(send_flags) | Ip4Flags(fragment_offset / 8);
            ip4_header.set(Ip4Header::FlagsOffset(), flags_offset);
            ip4_header.set(Ip4Header::HeaderChksum(), 0);
            
            // Calculate the IP header checksum.
            // Not inline since fragmentation is uncommon, better save program space.
            std::uint16_t calc_chksum = IpChksum(ip4_header.data, Ip4Header::Size);
            ip4_header.set(Ip4Header::HeaderChksum(), calc_chksum);
            
            // Construct a packet with header and partial data.
            IpBufNode data_node = dgram.toNode();
            IpBufNode header_node;
            IpBufRef frag_pkt = pkt.subHeaderToContinuedBy(
                Ip4Header::Size, &data_node, pkt_send_len, &header_node);
            
            // Send the packet to the driver.
            err = route_info.iface->m_params.send_ip4_packet(
                frag_pkt, route_info.addr, retryReq);
            
            // If this was the last fragment or there was an error, return.
            if ((send_flags & IpFlagsToSendFlags(Ip4Flags::MF)) == EnumZero ||
                AIPSTACK_UNLIKELY(err != IpErr::SUCCESS))
            {
                return err;
            }
            
            // Update the fragment offset and skip the sent data.
            std::uint16_t data_sent = pkt_send_len - Ip4Header::Size;
            fragment_offset += data_sent;
            dgram.skipBytes(data_sent);
        }
    }
    
public:
    /**
     * Prepare for sending multiple datagrams with similar header fields.
     * 
     * This determines routing information, fills in common header fields and
     * stores internal information into the given @ref IpSendPreparedIp4 structure.
     * After this is successful, @ref sendIp4DgramFast can be used to send multiple
     * datagrams in succession, with IP header fields as specified here.
     * 
     * This mechanism is intended for bulk transmission where performance is desired.
     * Fragmentation or forcing an interface are not supported.
     * 
     * Sending to a broadcast address or from a non-local address is only permitted with
     * specific flags in `send_flags`; see @ref sendIp4Dgram for details.
     * 
     * @param addrs Source and destination address.
     * @param ttl_proto The TTL and protocol fields combined.
     * @param header_end_ptr Pointer to the end of the IPv4 header (and start of data).
     *                       This must be the same location as for subsequent datagrams.
     * @param send_flags IP flags to send. All flags declared in @ref IpSendFlags are
     *        allowed (other bits must not be present).
     * @param prep Internal information is stored into this structure.
     * @return Success or error code.
     */
    AIPSTACK_ALWAYS_INLINE
    IpErr prepareSendIp4Dgram (Ip4AddrPair const &addrs, Ip4TtlProto ttl_proto,
                               char *header_end_ptr, IpSendFlags send_flags,
                               IpSendPreparedIp4<Arg> &prep)
    {
        AIPSTACK_ASSERT((send_flags & ~IpSendFlags::AllFlags) == EnumZero)
        
        // Get routing information (fill in route_info).
        if (AIPSTACK_UNLIKELY(!routeIp4(addrs.remote_addr, prep.route_info))) {
            return IpErr::NO_IP_ROUTE;
        }
        
        // Check if sending is allowed.
        IpErr check_err = checkSendIp4Allowed(addrs, send_flags, prep.route_info.iface);
        if (AIPSTACK_UNLIKELY(check_err != IpErr::SUCCESS)) {
            return check_err;
        }

        // Write IP header fields and calculate partial header checksum inline...
        auto ip4_header = Ip4Header::MakeRef(header_end_ptr - Ip4Header::Size);
        IpChksumAccumulator chksum;
        
        std::uint16_t version_ihl_dscp_ecn = std::uint16_t((4 << Ip4VersionShift) | 5) << 8;
        chksum.addWord(WrapType<std::uint16_t>(), version_ihl_dscp_ecn);
        ip4_header.set(Ip4Header::VersionIhlDscpEcn(), version_ihl_dscp_ecn);
        
        Ip4Flags flags_offset = IpFlagsInSendFlags(send_flags);
        chksum.addWord(WrapType<std::uint16_t>(), ToUnderlyingType(flags_offset));
        ip4_header.set(Ip4Header::FlagsOffset(), flags_offset);
        
        chksum.addWord(WrapType<std::uint16_t>(), ttl_proto.value());
        ip4_header.set(Ip4Header::TtlProto(), ttl_proto.value());
        
        chksum.addWord(WrapType<std::uint32_t>(), addrs.local_addr.value());
        ip4_header.set(Ip4Header::SrcAddr(), addrs.local_addr);
        
        chksum.addWord(WrapType<std::uint32_t>(), addrs.remote_addr.value());
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
     *             only valid temporarily (see the note in @ref IpSendPreparedIp4).
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
    IpErr sendIp4DgramFast (IpSendPreparedIp4<Arg> const &prep, IpBufRef dgram,
                            IpSendRetryRequest *retryReq)
    {
        AIPSTACK_ASSERT(dgram.tot_len <= TypeMax<std::uint16_t>())
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
        
        chksum.addWord(WrapType<std::uint16_t>(), std::uint16_t(pkt.tot_len));
        ip4_header.set(Ip4Header::TotalLen(), std::uint16_t(pkt.tot_len));
        
        std::uint16_t ident = m_next_id++; // generate identification number
        chksum.addWord(WrapType<std::uint16_t>(), ident);
        ip4_header.set(Ip4Header::Ident(), ident);
        
        // Set the IP header checksum.
        ip4_header.set(Ip4Header::HeaderChksum(), chksum.getChksum());
        
        // Send the packet to the driver.
        return prep.route_info.iface->m_params.send_ip4_packet(
            pkt, prep.route_info.addr, retryReq);
    }

private:
    inline static IpErr checkSendIp4Allowed (
        Ip4AddrPair const &addrs, IpSendFlags send_flags, Iface *iface)
    {
        if (AIPSTACK_LIKELY((send_flags & IpSendFlags::AllowBroadcastFlag) == EnumZero)) {
            if (AIPSTACK_UNLIKELY(addrs.remote_addr.isAllOnes())) {
                return IpErr::BCAST_REJECTED;
            }

            if (AIPSTACK_LIKELY(iface->m_have_addr)) {
                if (AIPSTACK_UNLIKELY(addrs.remote_addr == iface->m_addr.bcastaddr)) {
                    return IpErr::BCAST_REJECTED;
                }
            }
        }

        if (AIPSTACK_LIKELY((send_flags & IpSendFlags::AllowNonLocalSrc) == EnumZero)) {
            if (AIPSTACK_UNLIKELY(!iface->m_have_addr ||
                                  addrs.local_addr != iface->m_addr.addr))
            {
                return IpErr::NONLOCAL_SRC;
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
    bool routeIp4 (Ip4Addr dst_addr, IpRouteInfoIp4<Arg> &route_info) const
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
    bool routeIp4ForceIface (Ip4Addr dst_addr, IpIface<Arg> *iface,
                             IpRouteInfoIp4<Arg> &route_info)
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
     * If the Path MTU estimate was lowered, then all existing @ref IpMtuRef setup
     * for this address are notified (@ref IpMtuRef::pmtuChanged are called),
     * directly from this function.
     * 
     * @param remote_addr Address to which the ICMP message applies.
     * @param mtu_info The next-hop-MTU from the ICMP message.
     * @return True if the Path MTU estimate was lowered, false if not.
     */
    inline bool handleIcmpPacketTooBig (Ip4Addr remote_addr, std::uint16_t mtu_info)
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
     * If the Path MTU estimate was lowered, then all existing @ref IpMtuRef setup
     * for this address are notified (@ref IpMtuRef::pmtuChanged are called),
     * directly from this function.
     * 
     * @param remote_addr Address for which to check the Path MTU estimate.
     * @return True if the Path MTU estimate was lowered, false if not.
     */
    inline bool handleLocalPacketTooBig (Ip4Addr remote_addr)
    {
        return m_path_mtu_cache.handlePacketTooBig(remote_addr, TypeMax<std::uint16_t>());
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
    static bool checkUnicastSrcAddr (IpRxInfoIp4<Arg> const &ip_info)
    {
        return !ip_info.src_addr.isAllOnesOrMulticast() &&
               !ip_info.iface->ip4AddrIsLocalBcast(ip_info.src_addr);
    }
    
    /**
     * Send an IPv4 ICMP Destination Unreachable message.
     * 
     * This function must only be used in the context of a @ref
     * IpProtocolHandlerStub::recvIp4Dgram "recvIp4Dgram" function of a protocol handler,
     * to send an ICMP message triggered by the received IPv4 datagram being processed. The
     * `rx_ip_info` and `rx_dgram` arguments must be the exact values provided by @ref
     * IpStack, except that `rx_dgram` may be a truncation of the original.
     * 
     * The IP header and up to 8 data bytes of the received datagram that triggered this
     * ICMP message will be included, as required by RFC 792.
     * 
     * This function simply constructs the ICMP message and sends it using @ref
     * sendIp4Dgram. Restrictions and other considerations for that function apply here as
     * well.
     * 
     * @param rx_ip_info Information about the received datagram which triggered this ICMP
     *        message.
     * @param rx_dgram IP payload of the datagram which triggered this ICMP message. Note
     *        that this function expects to find the IPv4 header before this data (it knows
     *        how far back the header starts via @ref IpRxInfoIp4::header_len).
     * @param du_meta Information about the Destination Unreachable message.
     * @return Success or error code.
     */
    IpErr sendIp4DestUnreach (IpRxInfoIp4<Arg> const &rx_ip_info, IpBufRef rx_dgram,
                              Ip4DestUnreachMeta const &du_meta)
    {
        AIPSTACK_ASSERT(rx_dgram.offset >= rx_ip_info.header_len)

        // Build the Ip4AddrPair with IP addresses for sending.
        Ip4AddrPair addrs = {rx_ip_info.dst_addr, rx_ip_info.src_addr};

        // Calculate how much of the original datagram we will send.
        std::size_t data_len =
            std::size_t(rx_ip_info.header_len) + MinValue(std::size_t(8), rx_dgram.tot_len);
        
        // Get this data by revealing the IP header in rx_dgram and taking only the
        // calculated length.
        IpBufRef data = rx_dgram.revealHeaderMust(rx_ip_info.header_len).subTo(data_len);

        return sendIcmp4Message(addrs, rx_ip_info.iface, Icmp4Type::DestUnreach,
                                du_meta.icmp_code, du_meta.icmp_rest, data);
    }

    /**
     * Select an interface and local IP address to be used for communication with a
     * specific remote IP address.
     * 
     * This checks for a route to the given remote IP address and verifies that the
     * selected network interface has an IP address configured. If that is OK, it succeeds
     * and provides the interface and its local address.
     * 
     * @param remote_addr Remote IP address.
     * @param out_iface On success, is set to a pointer to the selected network interface
     *        (not changed on failure).
     * @param out_local_addr On success, is set to the selected local IP address (not
     *        changed on failure).
     * @return Success or error code.
     */
    IpErr selectLocalIp4Address (
        Ip4Addr remote_addr, IpIface<Arg> *&out_iface, Ip4Addr &out_local_addr) const
    {
        // Determine the local interface.
        IpRouteInfoIp4<Arg> route_info;
        if (!routeIp4(remote_addr, route_info)) {
            return IpErr::NO_IP_ROUTE;
        }
        
        // Determine the local IP address.
        IpIfaceIp4AddrSetting addr_setting = route_info.iface->getIp4Addr();
        if (!addr_setting.present) {
            return IpErr::NO_IP_ROUTE;
        }

        out_iface = route_info.iface;
        out_local_addr = addr_setting.addr;
        return IpErr::SUCCESS;
    }
    
private:
    using IfaceListenerList = LinkedList<
        MemberAccessor<IfaceListener, LinkedListNode<IfaceListenerLinkModel>,
                       &IfaceListener::m_list_node>,
        IfaceListenerLinkModel, false>;
    
    using IfaceList = LinkedList<
        MemberAccessor<Iface, LinkedListNode<IfaceLinkModel>, &Iface::m_iface_list_node>,
        IfaceLinkModel, false>;
    
    // Public works around access control issue from IpMtuRef with some compilers.
public:
#ifndef IN_DOXYGEN
    using BaseMtuRef = typename PathMtuCache::MtuRef;
#endif
    
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
        std::uint16_t version_ihl_dscp_ecn = ip4_header.get(Ip4Header::VersionIhlDscpEcn());
        chksum.addWord(WrapType<std::uint16_t>(), version_ihl_dscp_ecn);
        
        // Check IP version and header length...
        std::uint8_t version_ihl = version_ihl_dscp_ecn >> 8;
        std::uint8_t header_len;
        
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
        std::uint16_t total_len = ip4_header.get(Ip4Header::TotalLen());
        chksum.addWord(WrapType<std::uint16_t>(), total_len);
        
        // Check total length.
        if (AIPSTACK_UNLIKELY(total_len < header_len || total_len > pkt.tot_len)) {
            return;
        }
        
        // Create a reference to the payload.
        IpBufRef dgram = pkt.hideHeader(header_len).subTo(total_len - header_len);
        
        // Add ident and header checksum to checksum.
        chksum.addWord(WrapType<std::uint16_t>(), ip4_header.get(Ip4Header::Ident()));
        chksum.addWord(WrapType<std::uint16_t>(),
                       ip4_header.get(Ip4Header::HeaderChksum()));
        
        // Read TTL+protocol and add to checksum.
        Ip4TtlProto ttl_proto = ip4_header.get(Ip4Header::TtlProto());
        chksum.addWord(WrapType<std::uint16_t>(), ttl_proto.value());
        
        // Read addresses and add to checksum
        Ip4Addr src_addr = ip4_header.get(Ip4Header::SrcAddr());
        chksum.addWord(WrapType<std::uint32_t>(), src_addr.value());
        Ip4Addr dst_addr = ip4_header.get(Ip4Header::DstAddr());
        chksum.addWord(WrapType<std::uint32_t>(), dst_addr.value());
        
        // Get flags+offset and add to checksum.
        Ip4Flags flags_offset = ip4_header.get(Ip4Header::FlagsOffset());
        chksum.addWord(WrapType<std::uint16_t>(), ToUnderlyingType(flags_offset));
        
        // Verify IP header checksum.
        if (AIPSTACK_UNLIKELY(chksum.getChksum() != 0)) {
            return;
        }
        
        // Check if the more-fragments flag is set or the fragment offset is nonzero.
        if (AIPSTACK_UNLIKELY((flags_offset & (Ip4Flags::MF|Ip4Flags::OffsetMask)) != EnumZero)) {
            // Only accept fragmented packets which are unicasts to the
            // incoming interface address. This is to prevent filling up
            // our reassembly buffers with irrelevant packets. Note that
            // we don't check this for non-fragmented packets for
            // performance reasons, it generally up to protocol handlers.
            if (!iface->ip4AddrIsLocalAddr(dst_addr)) {
                return;
            }
            
            // Get the more-fragments flag and the fragment offset in bytes.
            bool more_fragments = (flags_offset & Ip4Flags::MF) != EnumZero;
            std::uint16_t fragment_offset =
                std::uint16_t(flags_offset & Ip4Flags::OffsetMask) * 8;
            
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
        
        // Create the IpRxInfoIp4 struct.
        IpRxInfoIp4<Arg> ip_info = {src_addr, dst_addr, ttl_proto, iface, header_len};

        // Do the real processing now that the datagram is complete and
        // sanity checked.
        recvIp4Dgram(ip_info, dgram);
    }
    
    static void recvIp4Dgram (IpRxInfoIp4<Arg> ip_info, IpBufRef dgram)
    {
        Ip4Protocol proto = ip_info.ttl_proto.proto();
        
        // Pass to interface listeners. If any listener accepts the
        // packet, inhibit further processing.
        for (IfaceListener *lis = ip_info.iface->m_listeners_list.first();
             lis != nullptr; lis = ip_info.iface->m_listeners_list.next(*lis))
        {
            if (lis->m_proto == proto) {
                if (AIPSTACK_UNLIKELY(lis->m_ip4_handler(ip_info, dgram))) {
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
                    static_cast<IpRxInfoIp4<Arg> const &>(ip_info),
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
        if (proto == Ip4Protocol::Icmp) {
            return recvIcmp4Dgram(ip_info, dgram);
        }
    }
    
    static void recvIcmp4Dgram (IpRxInfoIp4<Arg> const &ip_info, IpBufRef const &dgram)
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
        auto icmp4_header  = Icmp4Header::MakeRef(dgram.getChunkPtr());
        Icmp4Type type     = icmp4_header.get(Icmp4Header::Type());
        Icmp4Code code     = icmp4_header.get(Icmp4Header::Code());
        Icmp4RestType rest = icmp4_header.get(Icmp4Header::Rest());
        
        // Verify ICMP checksum.
        std::uint16_t calc_chksum = IpChksum(dgram);
        if (AIPSTACK_UNLIKELY(calc_chksum != 0)) {
            return;
        }
        
        // Get ICMP data by hiding the ICMP header.
        IpBufRef icmp_data = dgram.hideHeader(Icmp4Header::Size);
        
        IpStack *stack = ip_info.iface->m_stack;
        
        if (type == Icmp4Type::EchoRequest) {
            // Got echo request, send echo reply.
            // But if this is a broadcast request, respond only if allowed.
            if (is_broadcast_dst && !AllowBroadcastPing) {
                return;
            }
            stack->sendIcmp4EchoReply(rest, icmp_data, ip_info.src_addr, ip_info.iface);
        }
        else if (type == Icmp4Type::DestUnreach) {
            stack->handleIcmp4DestUnreach(code, rest, icmp_data, ip_info.iface);
        }
    }
    
    void sendIcmp4EchoReply (
        Icmp4RestType rest, IpBufRef data, Ip4Addr dst_addr, Iface *iface)
    {
        AIPSTACK_ASSERT(iface != nullptr)

        // Can only reply when we have an address assigned.
        if (AIPSTACK_UNLIKELY(!iface->m_have_addr)) {
            return;
        }
        
        Ip4AddrPair addrs = {iface->m_addr.addr, dst_addr};
        sendIcmp4Message(addrs, iface, Icmp4Type::EchoReply, Icmp4Code::Zero, rest, data);
    }

    IpErr sendIcmp4Message (Ip4AddrPair const &addrs, Iface *iface,
        Icmp4Type type, Icmp4Code code, Icmp4RestType rest, IpBufRef data)
    {
        // Allocate memory for headers.
        TxAllocHelper<Icmp4Header::Size, HeaderBeforeIp4Dgram>
            dgram_alloc(Icmp4Header::Size);
        
        // Write the ICMP header.
        auto icmp4_header = Icmp4Header::MakeRef(dgram_alloc.getPtr());
        icmp4_header.set(Icmp4Header::Type(),   type);
        icmp4_header.set(Icmp4Header::Code(),   code);
        icmp4_header.set(Icmp4Header::Chksum(), 0);
        icmp4_header.set(Icmp4Header::Rest(),   rest);
        
        // Construct the datagram reference with header and data.
        IpBufNode data_node = data.toNode();
        dgram_alloc.setNext(&data_node, data.tot_len);
        IpBufRef dgram = dgram_alloc.getBufRef();
        
        // Calculate ICMP checksum.
        std::uint16_t calc_chksum = IpChksum(dgram);
        icmp4_header.set(Icmp4Header::Chksum(), calc_chksum);
        
        // Send the datagram.
        return sendIp4Dgram(addrs, Ip4TtlProto{IcmpTTL, Ip4Protocol::Icmp},
            dgram, iface, nullptr, IpSendFlags());        
    }
    
    void handleIcmp4DestUnreach (
        Icmp4Code code, Icmp4RestType rest, IpBufRef icmp_data, Iface *iface)
    {
        // Check base IP header length.
        if (AIPSTACK_UNLIKELY(!icmp_data.hasHeader(Ip4Header::Size))) {
            return;
        }
        
        // Read IP header fields.
        auto ip4_header = Ip4Header::MakeRef(icmp_data.getChunkPtr());
        std::uint8_t version_ihl = ip4_header.get(Ip4Header::VersionIhlDscpEcn()) >> 8;
        std::uint16_t total_len  = ip4_header.get(Ip4Header::TotalLen());
        std::uint16_t ttl_proto  = ip4_header.get(Ip4Header::TtlProto());
        Ip4Addr src_addr         = ip4_header.get(Ip4Header::SrcAddr());
        Ip4Addr dst_addr         = ip4_header.get(Ip4Header::DstAddr());
        
        // Check IP version.
        if (AIPSTACK_UNLIKELY((version_ihl >> Ip4VersionShift) != 4)) {
            return;
        }
        
        // Check header length.
        // We require the entire header to fit into the first buffer.
        std::uint8_t header_len = (version_ihl & Ip4IhlMask) * 4;
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
        
        // Create the IpRxInfoIp4 struct.
        IpRxInfoIp4<Arg> ip_info = {src_addr, dst_addr, ttl_proto, iface, header_len};
        
        // Get the included IP data.
        std::size_t data_len = MinValueU(icmp_data.tot_len, total_len) - header_len;
        IpBufRef dgram_initial = icmp_data.hideHeader(header_len).subTo(data_len);
        
        // Dispatch based on the protocol.
        ListForBreak<ProtocolHelpersList>([&] AIPSTACK_TL(Helper, {
            if (ip_info.ttl_proto.proto() == Helper::IpProtocolNumber::Value) {
                Helper::get(this)->handleIp4DestUnreach(
                    static_cast<Ip4DestUnreachMeta const &>(du_meta),
                    static_cast<IpRxInfoIp4<Arg> const &>(ip_info),
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
    std::uint16_t m_next_id;
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
    AIPSTACK_OPTION_DECL_VALUE(HeaderBeforeIp, std::size_t, 14)
    
    /**
     * TTL of outgoing ICMP packets.
     */
    AIPSTACK_OPTION_DECL_VALUE(IcmpTTL, std::uint8_t, 64)
    
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
 * An @ref IpStack class type can be obtained as follows:
 * 
 * ```
 * using MyIpStackService = AIpStack::IpStackService<...options...>;
 * class MyIpStackArg : public MyIpStackService::template Compose<
 *     PlatformImpl, ProtocolServicesList> {};
 * using MyIpStack = AIpStack::IpStack<MyIpStackArg>;
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
     * Template to get the template parameter for @ref IpStack.
     * 
     * See @ref IpStackService for an example of instantiating the @ref IpStack.
     * It is advised to not pass this type directly to @ref IpStack but pass a dummy
     * user-defined class which inherits from it.
     * 
     * @tparam PlatformImpl_ Platform layer implementation, that is the PlatformImpl
     *         type to be used with @ref PlatformFacade.
     * @tparam ProtocolServicesList_ List of IP protocol handler services.
     *         For example, to support TCP and UDP, use
     *         `MakeTypeList\<IpTcpProtoService\<...\>, IpUdpProtoService\<...\>\>` with
     *         appropriate parameters passed to @ref IpTcpProtoService and @ref
     *         IpUdpProtoService.
     */
    template <typename PlatformImpl_, typename ProtocolServicesList_>
    struct Compose {
#ifndef IN_DOXYGEN
        using PlatformImpl = PlatformImpl_;
        using ProtocolServicesList = ProtocolServicesList_;
        using Params = IpStackService;

        // This is for completeness and is not typically used.
        AIPSTACK_DEF_INSTANCE(Compose, IpStack) 
#endif
    };
};

/** @} */

}

#endif
