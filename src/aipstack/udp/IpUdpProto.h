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

#ifndef AIPSTACK_IP_UDP_PROTO_H
#define AIPSTACK_IP_UDP_PROTO_H

#include <cstdint>
#include <cstddef>

#include <aipstack/meta/BasicMetaUtils.h>
#include <aipstack/misc/Use.h>
#include <aipstack/misc/NonCopyable.h>
#include <aipstack/misc/Assert.h>
#include <aipstack/misc/Hints.h>
#include <aipstack/misc/MinMax.h>
#include <aipstack/misc/IntRange.h>
#include <aipstack/misc/Function.h>
#include <aipstack/misc/EnumUtils.h>
#include <aipstack/structure/LinkedList.h>
#include <aipstack/structure/LinkModel.h>
#include <aipstack/structure/StructureRaiiWrapper.h>
#include <aipstack/structure/Accessor.h>
#include <aipstack/structure/LexiKeyCompare.h>
#include <aipstack/infra/Options.h>
#include <aipstack/infra/Instance.h>
#include <aipstack/infra/Err.h>
#include <aipstack/infra/Buf.h>
#include <aipstack/infra/Chksum.h>
#include <aipstack/infra/SendRetry.h>
#include <aipstack/proto/Ip4Proto.h>
#include <aipstack/proto/Udp4Proto.h>
#include <aipstack/proto/Icmp4Proto.h>
#include <aipstack/platform/PlatformFacade.h>
#include <aipstack/ip/IpAddr.h>
#include <aipstack/ip/IpStack.h>

namespace AIpStack {

#ifndef IN_DOXYGEN
template <typename Arg>
class UdpApi;

template <typename Arg>
class UdpListener;

template <typename Arg>
class UdpAssociation;

template <typename Arg>
class IpUdpProto;
#endif

template <typename Arg>
struct UdpListenParams {
    Ip4Addr iface_addr = Ip4Addr::ZeroAddr();
    std::uint16_t port = 0;
    bool accept_broadcast = false;
    bool accept_nonlocal_dst = false;
    IpIface<typename Arg::StackArg> *iface = nullptr;
};

template <typename Arg>
struct UdpRxInfo {
    std::uint16_t src_port;
    std::uint16_t dst_port;
    bool has_checksum;
};

enum class UdpRecvResult {
    Reject,
    AcceptContinue,
    AcceptStop
};

template <typename Arg>
struct UdpTxInfo {
    std::uint16_t src_port;
    std::uint16_t dst_port;
};

struct UdpAssociationKey {
    Ip4Addr local_addr;
    Ip4Addr remote_addr;
    std::uint16_t local_port;
    std::uint16_t remote_port;
};

template <typename Arg>
struct UdpAssociationParams {
    UdpAssociationKey key;
    bool accept_nonlocal_dst = false;
};

template <typename Arg>
class UdpApi :
    private NonCopyable<UdpApi<Arg>>
{
    template <typename> friend class UdpListener;
    template <typename> friend class UdpAssociation;
    template <typename> friend class IpUdpProto;

    AIPSTACK_USE_VALS(Arg::Params, (UdpTTL))

private:
    inline IpUdpProto<Arg> & proto () {
        return static_cast<IpUdpProto<Arg> &>(*this);
    }

    inline IpUdpProto<Arg> const & proto () const {
        return static_cast<IpUdpProto<Arg> const &>(*this);
    }
    
    // Prevent construction except from IpUdpProto (which is a friend). The second
    // declaration disables aggregate construction.
    UdpApi () = default;
    UdpApi (int);

public:
    using StackArg = typename Arg::StackArg;

    using Listener = UdpListener<Arg>;

    using Association = UdpAssociation<Arg>;

    inline static constexpr std::size_t HeaderBeforeUdpData =
        IpStack<StackArg>::HeaderBeforeIp4Dgram + Udp4Header::Size;

    inline static constexpr std::size_t MaxUdpDataLenIp4 =
        TypeMax<std::uint16_t> - Udp4Header::Size;

    IpErr sendUdpIp4Packet (Ip4AddrPair const &addrs, UdpTxInfo<Arg> const &udp_info,
                            IpBufRef udp_data, IpIface<StackArg> *iface,
                            IpSendRetryRequest *retryReq, IpSendFlags send_flags)
    {
        AIPSTACK_ASSERT(udp_data.tot_len <= MaxUdpDataLenIp4);
        AIPSTACK_ASSERT(udp_data.offset >= Ip4Header::Size + Udp4Header::Size);

        // Reveal the UDP header.
        IpBufRef dgram = udp_data.revealHeaderMust(Udp4Header::Size);

        // Write the UDP header.
        auto udp_header = Udp4Header::MakeRef(dgram.getChunkPtr());
        udp_header.set(Udp4Header::SrcPort(),  udp_info.src_port);
        udp_header.set(Udp4Header::DstPort(),  udp_info.dst_port);
        udp_header.set(Udp4Header::Length(),   std::uint16_t(dgram.tot_len));
        udp_header.set(Udp4Header::Checksum(), 0);
        
        // Calculate UDP checksum.
        IpChksumAccumulator chksum_accum;
        chksum_accum.addWord(WrapType<std::uint32_t>(), addrs.local_addr.value());
        chksum_accum.addWord(WrapType<std::uint32_t>(), addrs.remote_addr.value());
        chksum_accum.addWord(WrapType<std::uint16_t>(), AsUnderlying(Ip4Protocol::Udp));
        chksum_accum.addWord(WrapType<std::uint16_t>(), std::uint16_t(dgram.tot_len));
        std::uint16_t checksum = chksum_accum.getChksum(dgram);
        if (checksum == 0) {
            checksum = TypeMax<std::uint16_t>;
        }
        udp_header.set(Udp4Header::Checksum(), checksum);
        
        // Send the datagram.
        return proto().m_stack->sendIp4Dgram(dgram, iface, retryReq,
            Ip4CommonSendParams{addrs, UdpTTL, Ip4Protocol::Udp, send_flags});
    }
};

template <typename Arg>
class UdpListener :
    private NonCopyable<UdpListener<Arg>>
{
    template <typename> friend class IpUdpProto;
    
    AIPSTACK_USE_TYPES(IpUdpProto<Arg>, (ListenersLinkModel))

public:
    using StackArg = typename Arg::StackArg;

    using UdpIp4PacketHandler = Function<UdpRecvResult(
        IpRxInfoIp4<StackArg> const &ip_info,
        UdpRxInfo<Arg> const &udp_info, IpBufRef udp_data)>;

    UdpListener (UdpIp4PacketHandler handler) :
        m_handler(handler),
        m_udp(nullptr)
    {}

    ~UdpListener ()
    {
        reset();
    }

    void reset ()
    {
        if (m_udp != nullptr) {
            if (m_udp->m_next_listener == this) {
                m_udp->m_next_listener = m_udp->m_listeners_list.next(*this);
            }
            m_udp->m_listeners_list.remove(*this);
            m_udp = nullptr;
        }
    }

    bool isListening () const
    {
        return m_udp != nullptr;
    }

    UdpApi<Arg> & getUdp () const
    {
        AIPSTACK_ASSERT(isListening());

        return *m_udp;
    }

    UdpListenParams<Arg> const & getListenParams () const
    {
        AIPSTACK_ASSERT(isListening());

        return m_params;
    }

    IpErr startListening (UdpApi<Arg> &udp, UdpListenParams<Arg> const &params)
    {
        AIPSTACK_ASSERT(!isListening());

        m_udp = &udp.proto();
        m_params = params;
        
        m_udp->m_listeners_list.prepend(*this);

        return IpErr::Success;
    }

private:
    bool incomingPacketMatches (
        IpRxInfoIp4<StackArg> const &ip_info, UdpRxInfo<Arg> const &udp_info,
        bool dst_is_iface_addr) const
    {
        AIPSTACK_ASSERT(dst_is_iface_addr ==
            ip_info.iface->ip4AddrIsLocalAddr(ip_info.dst_addr));

        if (m_params.port != 0 && udp_info.dst_port != m_params.port) {
            return false;
        }

        if (m_params.iface != nullptr && ip_info.iface != m_params.iface) {
            return false;
        }

        bool is_bcast =
            ip_info.dst_addr.isAllOnes() ||
            ip_info.iface->ip4AddrIsLocalBcast(ip_info.dst_addr);
        
        if (!m_params.accept_broadcast && is_bcast) {
            return false;
        }

        if (!m_params.accept_nonlocal_dst && !is_bcast && !dst_is_iface_addr) {
            return false;
        }

        if (!m_params.iface_addr.isZero() &&
            !ip_info.iface->ip4AddrIsLocalAddr(m_params.iface_addr))
        {
            return false;
        }

        return true;
    }

private:
    UdpIp4PacketHandler m_handler;
    LinkedListNode<ListenersLinkModel> m_list_node;
    IpUdpProto<Arg> *m_udp;
    UdpListenParams<Arg> m_params;
};

template <typename Arg>
class UdpAssociation :
    private NonCopyable<UdpAssociation<Arg>>
{
    template <typename> friend class IpUdpProto;
    
public:
    using StackArg = typename Arg::StackArg;

    using UdpIp4PacketHandler = Function<UdpRecvResult(
        IpRxInfoIp4<StackArg> const &ip_info,
        UdpRxInfo<Arg> const &udp_info, IpBufRef udp_data)>;
    
    UdpAssociation (UdpIp4PacketHandler handler) :
        m_handler(handler),
        m_udp(nullptr)
    {}

    ~UdpAssociation ()
    {
        reset();
    }

    void reset ()
    {
        if (m_udp != nullptr) {
            m_udp->m_associations_index.removeEntry(*this);
            m_udp = nullptr;
        }
    }

    bool isAssociated () const
    {
        return m_udp != nullptr;
    }

    UdpApi<Arg> & getApi () const
    {
        AIPSTACK_ASSERT(isAssociated());

        return *m_udp;
    }

    UdpAssociationParams<Arg> const & getAssociationParams () const
    {
        AIPSTACK_ASSERT(isAssociated());

        return m_params;
    }

    IpErr associate (UdpApi<Arg> &api, UdpAssociationParams<Arg> const &params)
    {
        AIPSTACK_ASSERT(!isAssociated());

        IpUdpProto<Arg> &udp = api.proto();

        m_params = params;

        if (m_params.key.local_addr.isZero()) {
            // Select the local IP address.
            IpIface<StackArg> *iface;
            IpErr select_err = udp.m_stack->selectLocalIp4Address(
                m_params.key.remote_addr, iface, m_params.key.local_addr);
            if (select_err != IpErr::Success) {
                return select_err;
            }
        }

        if (m_params.key.local_port == 0) {
            if (!udp.get_ephemeral_port(m_params.key)) {
                return IpErr::NoPortAvailable;
            }
        } else {
            if (!udp.m_associations_index.findEntry(m_params.key).isNull()) {
                return IpErr::AddrInUse;
            }
        }

        m_udp = &udp;

        m_udp->m_associations_index.addEntry(*this);

        return IpErr::Success;
    }

private:
    UdpIp4PacketHandler m_handler;
    typename IpUdpProto<Arg>::AssociationIndex::Node m_index_node;
    IpUdpProto<Arg> *m_udp;
    UdpAssociationParams<Arg> m_params;
};

#ifndef IN_DOXYGEN

template <typename Arg>
class IpUdpProto :
    private NonCopyable<IpUdpProto<Arg>>,
    private UdpApi<Arg>
{
    template <typename> friend class UdpApi;
    template <typename> friend class UdpListener;
    template <typename> friend class UdpAssociation;

    AIPSTACK_USE_VALS(Arg::Params, (UdpTTL, EphemeralPortFirst, EphemeralPortLast))
    AIPSTACK_USE_TYPES(Arg::Params, (UdpIndexService))
    AIPSTACK_USE_TYPES(Arg, (PlatformImpl, StackArg))

    static_assert(EphemeralPortFirst > 0);
    static_assert(EphemeralPortFirst <= EphemeralPortLast);

    using Platform = PlatformFacade<PlatformImpl>;

    inline static constexpr PortNum NumEphemeralPorts =
        EphemeralPortLast - EphemeralPortFirst + 1;

    struct ListenerListNodeAccessor;
    using ListenersLinkModel = PointerLinkModel<UdpListener<Arg>>;

    using ListenersList = LinkedList<
        ListenerListNodeAccessor, ListenersLinkModel, /*WithLast=*/false>;

    struct ListenerListNodeAccessor : public MemberAccessor<
        UdpListener<Arg>, LinkedListNode<ListenersLinkModel>,
        &UdpListener<Arg>::m_list_node> {};
    
    struct AssociationIndexNodeAccessor;
    struct AssociationIndexKeyFuncs;
    using AssociationLinkModel = PointerLinkModel<UdpAssociation<Arg>>;
    using AssociationIndexLookupKeyArg = UdpAssociationKey const &;

    AIPSTACK_MAKE_INSTANCE(AssociationIndex, (UdpIndexService::template Index<
        AssociationIndexNodeAccessor, AssociationIndexLookupKeyArg,
        AssociationIndexKeyFuncs, AssociationLinkModel, /*Duplicates=*/false>))

    struct AssociationIndexNodeAccessor : public MemberAccessor<
        UdpAssociation<Arg>, typename AssociationIndex::Node,
        &UdpAssociation<Arg>::m_index_node> {};
    
    using AssociationKeyCompare = LexiKeyCompare<UdpAssociationKey, MakeTypeList<
        WrapValue<std::uint16_t UdpAssociationKey::*, &UdpAssociationKey::remote_port>,
        WrapValue<Ip4Addr UdpAssociationKey::*, &UdpAssociationKey::remote_addr>,
        WrapValue<std::uint16_t UdpAssociationKey::*, &UdpAssociationKey::local_port>,
        WrapValue<Ip4Addr UdpAssociationKey::*, &UdpAssociationKey::local_addr>
    >>;
    
    struct AssociationIndexKeyFuncs : public AssociationKeyCompare {
        inline static UdpAssociationKey const & GetKeyOfEntry (
            UdpAssociation<Arg> const &assoc)
        {
            return assoc.m_params.key;
        }
    };

public:
    IpUdpProto (IpProtocolHandlerArgs<StackArg> args) :
        m_stack(args.stack),
        m_next_listener(nullptr),
        m_next_ephemeral_port(EphemeralPortFirst)
    {}

    ~IpUdpProto ()
    {
        AIPSTACK_ASSERT(m_listeners_list.isEmpty());
        AIPSTACK_ASSERT(m_associations_index.isEmpty());
        AIPSTACK_ASSERT(m_next_listener == nullptr);
    }

    inline UdpApi<Arg> & getApi ()
    {
        return *this;
    }

    void recvIp4Dgram (IpRxInfoIp4<StackArg> const &ip_info, IpBufRef dgram)
    {
        // Check that there is a UDP header.
        if (AIPSTACK_UNLIKELY(!dgram.hasHeader(Udp4Header::Size))) {
            return;
        }
        auto udp_header = Udp4Header::MakeRef(dgram.getChunkPtr());

        // Fill in UdpRxInfo (has_checksum would be set later).
        UdpRxInfo<Arg> udp_info;
        udp_info.src_port = udp_header.get(Udp4Header::SrcPort());
        udp_info.dst_port = udp_header.get(Udp4Header::DstPort());

        // Check UDP length.
        std::uint16_t udp_length = udp_header.get(Udp4Header::Length());
        if (AIPSTACK_UNLIKELY(udp_length < Udp4Header::Size ||
                              udp_length > dgram.tot_len))
        {
            return;
        }
        
        // Truncate datagram to UDP length.
        dgram = dgram.subTo(udp_length);

        // We will remember whether the destination address is the address of the incoming
        // network interface. By default this is a precondition for dispatching the packet
        // to associations or listeners, but those can disable this requirement.
        bool dst_is_iface_addr;
        
        // This lambda calculates dst_is_iface_addr. It is called once here and possibly
        // again after application callbacks. The latter is due to the possibility that a
        // callback changs he interface address, and is critical for the assert in
        // UdpListener::incomingPacketMatches.
        auto updateCachedInfo = [&]() {
            dst_is_iface_addr = ip_info.iface->ip4AddrIsLocalAddr(ip_info.dst_addr);
        };
        updateCachedInfo();

        // We will verify the checksum when we find the first matching listener.
        bool checksum_verified = false;

        // This lambda function is used to verify the checksum on demand.
        auto verifyChecksumOnDemand = [&]() -> bool {
            if (!checksum_verified) {
                if (!verifyChecksum(ip_info, udp_header, dgram, udp_info.has_checksum)) {
                    // Bad checksum, calling code should drop the packet.
                    return false;
                }
                checksum_verified = true;
            }
            return true;
        };

        // We will remember if any association or listener accepted the packet.
        bool accepted = false;

        // Check if the packet should be dispatched to a listener.
        UdpAssociationKey assoc_key =
            {ip_info.dst_addr, ip_info.src_addr, udp_info.dst_port, udp_info.src_port};
        UdpAssociation<Arg> *assoc = m_associations_index.findEntry(assoc_key);

        if (assoc != nullptr) do {
            AIPSTACK_ASSERT(assoc->m_udp == this);

            // Check any accept_nonlocal_dst requirement.
            if (!assoc->m_params.accept_nonlocal_dst && !dst_is_iface_addr) {
                continue;
            }

            if (!verifyChecksumOnDemand()) {
                // Bad checksum, drop packet.
                return;
            }

            // Pass the packet to the association.
            IpBufRef udp_data = dgram.hideHeader(Udp4Header::Size);
            UdpRecvResult recv_result = assoc->m_handler(ip_info, udp_info, udp_data);
            
            // If the association wants that we don't pass the packet to any listener, then
            // return here.
            if (recv_result == UdpRecvResult::AcceptStop) {
                return;
            }

            // Remember if the association accepted the packet.
            if (recv_result != UdpRecvResult::Reject) {
                accepted = true;
            }

            // Consider possible interface configuration changes.
            updateCachedInfo();
        } while (false);
        
        // Look for listeners which match the incoming packet.
        // NOTE: `lis` must be properly adjusted at the end of each iteration!
        for (UdpListener<Arg> *lis = m_listeners_list.first(); lis != nullptr;) {
            AIPSTACK_ASSERT(lis->m_udp == this);
            
            // Check if the listener matches, if not skip it.
            if (!lis->incomingPacketMatches(ip_info, udp_info, dst_is_iface_addr)) {
                lis = m_listeners_list.next(*lis);
                continue;
            }

            if (!verifyChecksumOnDemand()) {
                // Bad checksum, drop packet.
                return;
            }

            // Set the m_next_listener pointer to the next listener on the list (if any).
            // In case the following callback resets (or destructs) the next listener,
            // UdpListener::reset() will advance m_next_listener so that we can safely
            // continue iterating.
            AIPSTACK_ASSERT(m_next_listener == nullptr);
            m_next_listener = m_listeners_list.next(*lis);

            // Pass the packet to the listener.
            IpBufRef udp_data = dgram.hideHeader(Udp4Header::Size);
            UdpRecvResult recv_result = lis->m_handler(ip_info, udp_info, udp_data);

            // Update `lis` to the next listener (if any) and clear m_next_listener.
            lis = m_next_listener;
            m_next_listener = nullptr;

            // If the listener wants that we don't pass the packet to any further listener,
            // then return here.
            if (recv_result == UdpRecvResult::AcceptStop) {
                return;
            }

            // Remember if the listener accepted the packet.
            if (recv_result != UdpRecvResult::Reject) {
                accepted = true;                
            }

            // Consider possible interface configuration changes.
            updateCachedInfo();
        }

        // If no association or listener has accepted the datagram and it is for our IP
        // address, we should send an ICMP message.
        if (!accepted && dst_is_iface_addr) {
            if (!verifyChecksumOnDemand()) {
                // Bad checksum, drop packet.
                return;
            }

            // Send an ICMP Destination Unreachable, Port Unreachable message.
            Ip4DestUnreachMeta du_meta{Icmp4Code::DestUnreachPortUnreach, Icmp4RestType()};
            m_stack->sendIp4DestUnreach(ip_info, dgram, du_meta);
        }
    }

    void handleIp4DestUnreach (
        [[maybe_unused]] Ip4DestUnreachMeta const &du_meta,
        [[maybe_unused]] IpRxInfoIp4<StackArg> const &ip_info,
        [[maybe_unused]] IpBufRef dgram_initial)
    {
    }

private:
    static bool verifyChecksum (
        IpRxInfoIp4<StackArg> const &ip_info, Udp4Header::Ref udp_header,
        IpBufRef dgram, bool &has_checksum)
    {
        std::uint16_t checksum = udp_header.get(Udp4Header::Checksum());

        has_checksum = (checksum != 0);

        if (has_checksum) {
            IpChksumAccumulator chksum_accum;
            chksum_accum.addWord(WrapType<std::uint32_t>(), ip_info.src_addr.value());
            chksum_accum.addWord(WrapType<std::uint32_t>(), ip_info.dst_addr.value());
            chksum_accum.addWord(WrapType<std::uint16_t>(), AsUnderlying(Ip4Protocol::Udp));
            chksum_accum.addWord(WrapType<std::uint16_t>(), std::uint16_t(dgram.tot_len));

            if (chksum_accum.getChksum(dgram) != 0) {
                return false;
            }
        }

        return true;
    }

    bool get_ephemeral_port (UdpAssociationKey &key)
    {
        for ([[maybe_unused]] PortNum i : IntRange(NumEphemeralPorts)) {
            PortNum port = m_next_ephemeral_port;
            m_next_ephemeral_port = (port < EphemeralPortLast) ?
                (port + 1) : EphemeralPortFirst;
            
            key.local_port = port;

            if (m_associations_index.findEntry(key).isNull()) {
                return true;
            }
        }
        
        return false;
    }
    
private:
    IpStack<StackArg> *m_stack;
    StructureRaiiWrapper<ListenersList> m_listeners_list;
    StructureRaiiWrapper<typename AssociationIndex::Index> m_associations_index;
    UdpListener<Arg> *m_next_listener;
    PortNum m_next_ephemeral_port;
};

#endif

struct IpUdpProtoOptions {
    AIPSTACK_OPTION_DECL_VALUE(UdpTTL, std::uint8_t, 64)
    AIPSTACK_OPTION_DECL_VALUE(EphemeralPortFirst, std::uint16_t, 49152)
    AIPSTACK_OPTION_DECL_VALUE(EphemeralPortLast, std::uint16_t, 65535)
    AIPSTACK_OPTION_DECL_TYPE(UdpIndexService, void)
};

template <typename ...Options>
class IpUdpProtoService {
    template <typename> friend class IpUdpProto;
    template <typename> friend class UdpApi;
    
    AIPSTACK_OPTION_CONFIG_VALUE(IpUdpProtoOptions, UdpTTL)
    AIPSTACK_OPTION_CONFIG_VALUE(IpUdpProtoOptions, EphemeralPortFirst)
    AIPSTACK_OPTION_CONFIG_VALUE(IpUdpProtoOptions, EphemeralPortLast)
    AIPSTACK_OPTION_CONFIG_TYPE(IpUdpProtoOptions, UdpIndexService)
    
public:
    // This tells IpStack which IP protocol we receive packets for.
    using IpProtocolNumber = WrapValue<Ip4Protocol, Ip4Protocol::Udp>;
    
#ifndef IN_DOXYGEN
    template <typename PlatformImpl_, typename StackArg_>
    struct Compose {
        using PlatformImpl = PlatformImpl_;
        using StackArg = StackArg_;
        using Params = IpUdpProtoService;
        AIPSTACK_DEF_INSTANCE(Compose, IpUdpProto)
    };
#endif
};

}

#endif
