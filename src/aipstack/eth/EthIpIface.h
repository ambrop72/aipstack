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

#ifndef AIPSTACK_ETH_IP_IFACE_H
#define AIPSTACK_ETH_IP_IFACE_H

#include <cstddef>
#include <cstdint>

#include <aipstack/meta/ChooseInt.h>
#include <aipstack/misc/Assert.h>
#include <aipstack/misc/IntRange.h>
#include <aipstack/misc/Use.h>
#include <aipstack/misc/Hints.h>
#include <aipstack/misc/NonCopyable.h>
#include <aipstack/misc/OneOf.h>
#include <aipstack/misc/MinMax.h>
#include <aipstack/misc/Function.h>
#include <aipstack/structure/LinkModel.h>
#include <aipstack/structure/LinkedList.h>
#include <aipstack/structure/StructureRaiiWrapper.h>
#include <aipstack/structure/TimerQueue.h>
#include <aipstack/structure/Accessor.h>
#include <aipstack/infra/Struct.h>
#include <aipstack/infra/Buf.h>
#include <aipstack/infra/SendRetry.h>
#include <aipstack/infra/TxAllocHelper.h>
#include <aipstack/infra/Err.h>
#include <aipstack/infra/Options.h>
#include <aipstack/infra/ObserverNotification.h>
#include <aipstack/infra/Instance.h>
#include <aipstack/proto/EthernetProto.h>
#include <aipstack/proto/ArpProto.h>
#include <aipstack/ip/IpAddr.h>
#include <aipstack/ip/IpStack.h>
#include <aipstack/eth/MacAddr.h>
#include <aipstack/eth/EthHw.h>
#include <aipstack/platform/PlatformFacade.h>

namespace AIpStack {

/**
 * @addtogroup eth
 * @{
 */

/**
 * Encapsulates state information provided by Ethernet network interface drivers.
 * 
 * Structures of this type are returned by @ref EthIfaceDriverParams::get_eth_state.
 */
struct EthIfaceState {
    /**
     * Whether the link is up.
     */
    bool link_up;
};

/**
 * Encapsulates interface parameters passed to the @ref EthIpIface constructor.
 */
struct EthIfaceDriverParams {
    /**
     * Maximum frame size including the 14-byte Ethernet header.
     * 
     * The resulting IP MTU (14 bytes less) must be at least @ref IpStack::MinMTU.
     */
    std::size_t eth_mtu = 0;
    
    /**
     * Pointer to the MAC address of the network interface.
     * 
     * This must point to a MAC address whose lifetime exceeds that of the
     * @ref EthIpIface and which does not change in that time.
     */
    MacAddr const *mac_addr = nullptr;

    /**
     * Driver function to send an Ethernet frame through the interface.
     * 
     * @note This function must be provided.
     * 
     * This is called whenever an Ethernet frame needs to be sent. The driver should
     * copy the frame as needed because it must not access the referenced buffers
     * outside this function.
     * 
     * @param frame Frame to send, this includes the Ethernet header. It is guaranteed
     *        that its size does not exceed the @ref eth_mtu specified at construction.
     *        Frames originating from @ref EthIpIface itself will always have at least
     *        @ref EthIpIfaceOptions::HeaderBeforeEth bytes available before the Ethernet
     *        header for any lower-layer headers, but for frames with IP payload it is up
     *        to the application to include that in @ref IpStackOptions::HeaderBeforeIp.
     * @return Success or error code. The @ref EthIpIface does not itself check for any
     *         specific error code but the error code may be propagated to the IP layer
     *         (@ref IpStack) which may do so.
     */
    Function<IpErr(IpBufRef frame)> send_frame = nullptr;
    
    /**
     * Driver function to get the drived-provided interface state.
     * 
     * @note This function must be provided.
     * 
     * The driver should call @ref EthIpIface::ethStateChanged whenever the state that
     * would be returned here has changed.
     * 
     * @return Driver-provided-state (currently just the link-up flag).
     */
    Function<EthIfaceState()> get_eth_state = nullptr;
};

/**
 * Ethernet-based network interface.
 * 
 * This class is an abstract IP-layer network interface driver for Ethernet-based
 * interfaces. It internally uses @ref IpDriverIface to interact with the IP layer
 * (@ref IpStack), while defining an API of its for interaction with the Ethernet-layer
 * driver. It also implements the @ref EthHwIface hardware-type-specific interface (see
 * the @ref eth-hw module).
 * 
 * This class is used as follows:
 * - The Ethernet-layer driver constructs and takes ownership of an @ref EthIpIface
 *   instance when initializing a network interface (multiple instances can be
 *   constructed of course).
 * - The driver implements the driver functions passed via @ref
 *   EthIfaceDriverParams::send_frame (which sends a frame) and @ref
 *   EthIfaceDriverParams::get_eth_state (which returns the interface state).
 * - The driver calls the functions @ref recvFrame (when a frame is received) and
 *   @ref ethStateChanged (when the state may have changed).
 * - The driver exposes the @ref IpIface to to the application and the application
 *   uses that to configure and control the interface on the IP layer (the driver can
 *   get the @ref IpIface by calling @ref iface()).
 * 
 * This class internally maintains an ARP cache, which is interface-specific. Note that if
 * there is no useful ARP cache entry for an outgoing IP packet, this class will (typically)
 * return the @ref IpErr::ArpQueryInProgress error code from @ref
 * IpIfaceDriverParams::send_ip4_packet and start an ARP resolution process. It makes an
 * effort to inform the caller when the resolution is successful through the @ref
 * send-retry "send-retry" mechanism so that it can retry sending, but such notification
 * is not guaranteed.
 * 
 * @tparam Arg An instantiation of the @ref EthIpIfaceService::Compose template
 *         or a dummy class derived from such; see @ref EthIpIfaceService for an
 *         example.
 */
template <typename Arg>
class EthIpIface final :
    private NonCopyable<EthIpIface<Arg>>
#ifndef IN_DOXYGEN
    ,private EthHwIface
#endif
{
    AIPSTACK_USE_VALS(Arg::Params, (NumArpEntries, ArpProtectCount, HeaderBeforeEth))
    AIPSTACK_USE_TYPES(Arg::Params, (TimersStructureService))
    AIPSTACK_USE_TYPES(Arg, (PlatformImpl, StackArg))
    
    using Platform = PlatformFacade<PlatformImpl>;
    AIPSTACK_USE_TYPES(Platform, (TimeType))
    
    inline static constexpr std::size_t EthArpPktSize =
        EthHeader::Size + ArpIp4Header::Size;
    
    // Sanity check ARP table configuration.
    static_assert(NumArpEntries > 0);
    static_assert(ArpProtectCount >= 0);
    static_assert(ArpProtectCount <= NumArpEntries);
    
    inline static constexpr int ArpNonProtectCount = NumArpEntries - ArpProtectCount;
    
    // Get an unsigned integer type sufficient for ARP entry indexes and null value.
    using ArpEntryIndexType = ChooseIntForMax<NumArpEntries, false>;
    inline static constexpr ArpEntryIndexType ArpEntryNull = TypeMax<ArpEntryIndexType>;
    
    // Number of ARP resolution attempts in the Query and Refreshing states.
    inline static constexpr std::uint8_t ArpQueryAttempts = 3;
    inline static constexpr std::uint8_t ArpRefreshAttempts = 2;
    
    // These need to fit in 4 bits available in ArpEntry::attempts_left.
    static_assert(ArpQueryAttempts <= 15);
    static_assert(ArpRefreshAttempts <= 15);
    
    // Base ARP response timeout, doubled for each retransmission.
    inline static constexpr TimeType ArpBaseResponseTimeoutTicks = 1.0 * Platform::TimeFreq;
    
    // Time after a Valid entry will go to Refreshing when used.
    inline static constexpr TimeType ArpValidTimeoutTicks = 60.0 * Platform::TimeFreq;
    
    struct ArpEntry;
    struct ArpEntryTimerQueueNodeUserData;
    struct ArpEntriesAccessor;
    
    // Get the TimerQueueService type.
    using TheTimerQueueService = TimerQueueService<TimersStructureService>;
    
    // Link model for ARP entry data structures.
    //struct ArpEntriesLinkModel = PointerLinkModel<ArpEntry> {};
    struct ArpEntriesLinkModel : public ArrayLinkModelWithAccessor<
        ArpEntry, ArpEntryIndexType, ArpEntryNull, EthIpIface, ArpEntriesAccessor> {};
    using ArpEntryRef = typename ArpEntriesLinkModel::Ref;
    
    // Nodes in ARP entry data structures.
    using ArpEntryListNode = LinkedListNode<ArpEntriesLinkModel>;
    using ArpEntryTimerQueueNode = typename TheTimerQueueService::template Node<
        ArpEntriesLinkModel, TimeType, ArpEntryTimerQueueNodeUserData>;
    
    // ARP entry states.
    struct ArpEntryState { enum : std::uint8_t {Free, Query, Valid, Refreshing}; };
    
    // ARP entry states where the entry timer is allowed to be active.
    inline static auto one_of_timer_entry_states ()
    {
        return OneOf(ArpEntryState::Query, ArpEntryState::Valid, ArpEntryState::Refreshing);
    }
    
    struct ArpEntryTimerQueueNodeUserData {
        // Entry state (ArpEntryState::*).
        std::uint8_t state : 2;
        
        // Whether the entry is weak (seen by chance) or hard (needed at some point).
        // Free entries must be weak.
        bool weak : 1;
        
        // Whether the entry timer is active (inserted into the timer queue).
        bool timer_active : 1;
        
        // Query and Refreshing states: How many more response timeouts before the
        // entry becomes Free or Refreshing respectively.
        // Valid state: 1 if the timeout has not elapsed yet, 0 if it has.
        std::uint8_t attempts_left : 4;
    };
    
    // ARP table entry (in array m_arp_entries)
    struct ArpEntry {
        inline ArpEntryTimerQueueNodeUserData & nud()
        {
            return timer_queue_node;
        }
        
        // MAC address of the entry (valid in Valid and Refreshing states).
        MacAddr mac_addr;
        
        // Node in linked lists (m_used_entries_list or m_free_entries_list).
        ArpEntryListNode list_node;
        
        // Node in the timer queue (m_timer_queue).
        ArpEntryTimerQueueNode timer_queue_node;
        
        // IP address of the entry (valid in all states except Free).
        Ip4Addr ip_addr;
        
        // List of send-retry waiters to be notified when resolution is complete.
        IpSendRetryList retry_list;
    };
    
    // Accessors for data structure nodes.
    struct ArpEntryListNodeAccessor :
        public MemberAccessor<ArpEntry, ArpEntryListNode, &ArpEntry::list_node>{};
    struct ArpEntryTimerQueueNodeAccessor :
        public MemberAccessor<ArpEntry, ArpEntryTimerQueueNode,
                              &ArpEntry::timer_queue_node> {};
    
    // Linked list type.
    using ArpEntryList = LinkedList<
        ArpEntryListNodeAccessor, ArpEntriesLinkModel, true>;
    
    // Data structure type for ARP entry timers.
    using ArpEntryTimerQueue = typename TheTimerQueueService::template Queue<
        ArpEntriesLinkModel, ArpEntryTimerQueueNodeAccessor, TimeType,
        ArpEntryTimerQueueNodeUserData>;
    
public:
    /**
     * Construct the interface.
     * 
     * The driver must be careful to not perform any action that might result in calls
     * of driver functions (such as sending frames to this interface) until the driver
     * is able to handle these calls.
     * 
     * @param platform_ The platform facade (the same one that `stack` uses).
     * @param stack Pointer to the IP stack (must outlive this interface).
     * @param params Interface parameters, see @ref EthIfaceDriverParams.
     */
    EthIpIface (PlatformFacade<PlatformImpl> platform_, IpStack<StackArg> *stack,
                EthIfaceDriverParams const &params)
    :
        m_params(params),
        m_driver_iface(stack, IpIfaceDriverParams{
            /*ip_mtu=*/ std::size_t(params.eth_mtu - EthHeader::Size),
            /*hw_type=*/ IpHwType::Ethernet,
            /*hw_iface=*/ static_cast<EthHwIface *>(this),
            AIPSTACK_BIND_MEMBER_TN(&EthIpIface::driverSendIp4Packet, this),
            AIPSTACK_BIND_MEMBER_TN(&EthIpIface::driverGetState, this)
        }),
        m_timer(platform_, AIPSTACK_BIND_MEMBER_TN(&EthIpIface::timerHandler, this))
    {
        AIPSTACK_ASSERT(params.eth_mtu >= EthHeader::Size)
        AIPSTACK_ASSERT(params.mac_addr != nullptr)
        AIPSTACK_ASSERT(params.send_frame)
        AIPSTACK_ASSERT(params.get_eth_state)
        
        // Initialize ARP entries...
        for (auto &e : m_arp_entries) {
            // State Free, timer not active.
            e.nud().state = ArpEntryState::Free;
            e.nud().weak = false; // irrelevant, for efficiency
            e.nud().timer_active = false;
            e.nud().attempts_left = 0; // irrelevant, for efficiency
            
            // Insert to free list.
            m_free_entries_list.append({e, *this}, *this);
        }
    }

    /**
     * Destruct the interface.
     * 
     * @note There are restrictions regarding the context from which an interface
     * may be destructed; refer to @ref IpDriverIface destructor.
     * 
     * The driver must be careful to not perform any action that might result in calls
     * of driver functions (such as sending frames to this interface) after it is
     * no longer ready to handle these calls.
     */
    ~EthIpIface () = default;

    /**
     * Get the @ref IpIface representing this network interface.
     * 
     * @return Reference to the @ref IpIface.
     */
    inline IpIface<StackArg> & iface () {
        return m_driver_iface.iface();
    }
    
    /**
     * Process a received Ethernet frame.
     * 
     * This function should be called by the driver when an Ethernet frame is received.
     * 
     * @note The driver must support various driver functions being called from within
     * this, especially @ref EthIfaceDriverParams::send_frame.
     * 
     * @param frame Received frame, presumably starting with the Ethernet header. The
     *              referenced buffers will only be read from within this function call.
     */
    void recvFrame (IpBufRef frame)
    {
        // Check that we have an Ethernet header.
        if (AIPSTACK_UNLIKELY(!frame.hasHeader(EthHeader::Size))) {
            return;
        }
        
        // Store the reference to the Ethernet header (for getRxEthHeader).
        m_rx_eth_header = EthHeader::MakeRef(frame.getChunkPtr());
        
        // Get the EtherType.
        EthType ethtype = m_rx_eth_header.get(EthHeader::EthType());
        
        // Hide the header to get the payload.
        auto pkt = frame.hideHeader(EthHeader::Size);
        
        // Handle based on the EtherType.
        if (AIPSTACK_LIKELY(ethtype == EthType::Ipv4)) {
            m_driver_iface.recvIp4Packet(pkt);
        }
        else if (ethtype == EthType::Arp) {
            recvArpPacket(pkt);
        }
    }
    
    /**
     * Notify that the driver-provided state may have changed.
     * 
     * This should be called by the driver after any value that would be returned by
     * @ref EthIfaceDriverParams::get_eth_state has changed. It does not strictly have
     * to be called immediately after every change but it should be called soon after
     * a change.
     * 
     * @note The driver must support various driver functions being called from within
     * this, especially @ref EthIfaceDriverParams::send_frame.
     */
    inline void ethStateChanged ()
    {
        // Forward notification to IP stack.
        m_driver_iface.stateChanged();
    }
    
private:
    IpErr driverSendIp4Packet (IpBufRef pkt, Ip4Addr ip_addr,
                               IpSendRetryRequest *retryReq)
    {
        // Try to resolve the MAC address.
        MacAddr dst_mac;
        IpErr resolve_err = resolve_hw_addr(ip_addr, &dst_mac, retryReq);
        if (AIPSTACK_UNLIKELY(resolve_err != IpErr::Success)) {
            return resolve_err;
        }
        
        // Reveal the Ethernet header.
        IpBufRef frame;
        if (AIPSTACK_UNLIKELY(!pkt.revealHeader(EthHeader::Size, &frame))) {
            return IpErr::NoHeaderSpace;
        }
        
        // Write the Ethernet header.
        auto eth_header = EthHeader::MakeRef(frame.getChunkPtr());
        eth_header.set(EthHeader::DstMac(),  dst_mac);
        eth_header.set(EthHeader::SrcMac(),  *m_params.mac_addr);
        eth_header.set(EthHeader::EthType(), EthType::Ipv4);
        
        // Send the frame via the lower-layer driver.
        return m_params.send_frame(frame);
    }
    
    IpIfaceDriverState driverGetState ()
    {
        // Get the state from the lower-layer driver.
        EthIfaceState eth_state = m_params.get_eth_state();
        
        // Return the state based on that: copy link_up.
        IpIfaceDriverState state = {};
        state.link_up = eth_state.link_up;
        return state;
    }
    
private: // EthHwIface
    MacAddr getMacAddr () override final
    {
        return *m_params.mac_addr;
    }
    
    EthHeader::Ref getRxEthHeader () override final
    {
        return m_rx_eth_header;
    }
    
    IpErr sendArpQuery (Ip4Addr ip_addr) override final
    {
        return send_arp_packet(ArpOpType::Request, MacAddr::BroadcastAddr(), ip_addr);
    }
    
    EthArpObservable & getArpObservable () override final
    {
        return m_arp_observable;
    }
    
private:
    inline PlatformFacade<PlatformImpl> platform () const
    {
        return m_timer.platform();
    }

    void recvArpPacket (IpBufRef pkt)
    {
        // Check that we have the ARP header.
        if (AIPSTACK_UNLIKELY(!pkt.hasHeader(ArpIp4Header::Size))) {
            return;
        }
        auto arp_header = ArpIp4Header::MakeRef(pkt.getChunkPtr());
        
        // Sanity check ARP header.
        if (arp_header.get(ArpIp4Header::HwType())       != ArpHwType::Eth ||
            arp_header.get(ArpIp4Header::ProtoType())    != EthType::Ipv4  ||
            arp_header.get(ArpIp4Header::HwAddrLen())    != MacAddr::Size  ||
            arp_header.get(ArpIp4Header::ProtoAddrLen()) != Ip4Addr::Size)
        {
            return;
        }
        
        // Get some ARP header fields.
        ArpOpType op_type   = arp_header.get(ArpIp4Header::OpType());
        MacAddr src_mac     = arp_header.get(ArpIp4Header::SrcHwAddr());
        Ip4Addr src_ip_addr = arp_header.get(ArpIp4Header::SrcProtoAddr());
        
        // Try to save the hardware address.
        save_hw_addr(src_ip_addr, src_mac);
        
        // If this is an ARP request for our IP address, send a response.
        if (op_type == ArpOpType::Request) {
            IpIfaceIp4Addrs const *ifaddr = m_driver_iface.getIp4Addrs();
            if (ifaddr != nullptr &&
                arp_header.get(ArpIp4Header::DstProtoAddr()) == ifaddr->addr)
            {
                send_arp_packet(ArpOpType::Reply, src_mac, src_ip_addr);
            }
        }
    }
    
    AIPSTACK_ALWAYS_INLINE
    IpErr resolve_hw_addr (
        Ip4Addr ip_addr, MacAddr *mac_addr, IpSendRetryRequest *retryReq)
    {
        // First look if the first used entry is a match, as an optimization.
        ArpEntryRef entry_ref = m_used_entries_list.first(*this);
        
        if (AIPSTACK_LIKELY(!entry_ref.isNull() && (*entry_ref).ip_addr == ip_addr)) {
            // Fast path, the first used entry is a match.
            AIPSTACK_ASSERT((*entry_ref).nud().state != ArpEntryState::Free)
            
            // Make sure the entry is hard as get_arp_entry would do below.
            (*entry_ref).nud().weak = false;
        } else {
            // Slow path: use get_arp_entry, make a hard entry.
            GetArpEntryRes get_res = get_arp_entry(ip_addr, false, entry_ref);
            
            // Did we not get an (old or new) entry for this address?
            if (AIPSTACK_UNLIKELY(get_res != GetArpEntryRes::GotArpEntry)) {
                // If this is a broadcast IP address, return the broadcast MAC address.
                if (get_res == GetArpEntryRes::BroadcastAddr) {
                    *mac_addr = MacAddr::BroadcastAddr();
                    return IpErr::Success;
                } else {
                    // Failure, cannot get MAC address.
                    return IpErr::NoHardwareRoute;
                }
            }
        }
        
        ArpEntry &entry = *entry_ref;
        
        // Got a Valid or Refreshing entry?
        if (AIPSTACK_LIKELY(entry.nud().state >= ArpEntryState::Valid)) {
            // If it is a timed out Valid entry, transition to Refreshing.
            if (AIPSTACK_UNLIKELY(entry.nud().attempts_left == 0)) {
                // Refreshing entry never has attempts_left==0 so no need to check for
                // Valid state in the if. We have a Valid entry and the timer is also
                // not active (needed by set_entry_timer) since attempts_left==0 implies
                // that it has expired already
                AIPSTACK_ASSERT(entry.nud().state == ArpEntryState::Valid)
                AIPSTACK_ASSERT(!entry.nud().timer_active)
                
                // Go to Refreshing state, start timeout, send first unicast request.
                entry.nud().state = ArpEntryState::Refreshing;
                entry.nud().attempts_left = ArpRefreshAttempts;
                set_entry_timer(entry);
                update_timer();
                send_arp_packet(ArpOpType::Request, entry.mac_addr, entry.ip_addr);
            }
            
            // Success, return MAC address.
            *mac_addr = entry.mac_addr;
            return IpErr::Success;
        } else {
            // If this is a Free entry, initialize it.
            if (entry.nud().state == ArpEntryState::Free) {
                // Timer is not active for Free entries (needed by set_entry_timer).
                AIPSTACK_ASSERT(!entry.nud().timer_active)
                
                // Go to Query state, start timeout, send first broadcast request.
                // NOTE: Entry is already inserted to m_used_entries_list.
                entry.nud().state = ArpEntryState::Query;
                entry.nud().attempts_left = ArpQueryAttempts;
                set_entry_timer(entry);
                update_timer();
                send_arp_packet(ArpOpType::Request, MacAddr::BroadcastAddr(), ip_addr);
            }
            
            // Add a request to the retry list if a request is supplied.
            entry.retry_list.addRequest(retryReq);
            
            // Return ArpQueryInProgress error.
            return IpErr::ArpQueryInProgress;
        }
    }
    
    void save_hw_addr (Ip4Addr ip_addr, MacAddr mac_addr)
    {
        // Sanity check MAC address: not broadcast.
        if (AIPSTACK_UNLIKELY(mac_addr == MacAddr::BroadcastAddr())) {
            return;
        }
        
        // Get an entry, if a new entry is allocated it will be weak.
        ArpEntryRef entry_ref;
        GetArpEntryRes get_res = get_arp_entry(ip_addr, true, entry_ref);
        
        // Did we get an (old or new) entry for this address?
        if (get_res == GetArpEntryRes::GotArpEntry) {
            ArpEntry &entry = *entry_ref;
            
            // Set entry to Valid state, remember MAC address, start timeout.
            entry.nud().state = ArpEntryState::Valid;
            entry.mac_addr = mac_addr;
            entry.nud().attempts_left = 1;
            clear_entry_timer(entry); // set_entry_timer requires !timer_active
            set_entry_timer(entry);
            update_timer();
            
            // Dispatch send-retry requests.
            // NOTE: The handlers called may end up changing this ARP entry, including
            // reusing it for a different IP address. In that case retry_list.reset()
            // would be called from reset_arp_entry, but that is safe since
            // SentRetry::List supports it.
            entry.retry_list.dispatchRequests();
        }
        
        // Notify the ARP observers so long as the address is not obviously bad.
        // It is important to do this even if no ARP entry was obtained, since that
        // will not happen if the interface has no IP address configured, which is
        // exactly when DHCP needs to be notified.
        if (ip_addr != Ip4Addr::AllOnesAddr() && ip_addr != Ip4Addr::ZeroAddr()) {
            m_arp_observable.notifyKeepObservers([&](EthArpObserver &observer) {
                EthHwIface::notifyEthArpObserver(observer, ip_addr, mac_addr);
            });
        }
    }
    
    enum class GetArpEntryRes {GotArpEntry, BroadcastAddr, InvalidAddr};
    
    // NOTE: If a Free entry is obtained, then 'weak' and 'ip_addr' have been
    // set, the entry is already in m_used_entries_list, but the caller must
    // complete initializing it to a non-Free state. Also, update_timer is needed
    // afterward then.
    GetArpEntryRes get_arp_entry (Ip4Addr ip_addr, bool weak, ArpEntryRef &out_entry)
    {
        // Look for a used entry with this IP address while also collecting
        // some information to be used in case we don't find an entry...
        
        int num_hard = 0;
        ArpEntryRef last_weak_entry_ref = ArpEntryRef::null();
        ArpEntryRef last_hard_entry_ref = ArpEntryRef::null();
        
        ArpEntryRef entry_ref = m_used_entries_list.first(*this);
        
        while (!entry_ref.isNull()) {
            ArpEntry &entry = *entry_ref;
            AIPSTACK_ASSERT(entry.nud().state != ArpEntryState::Free)
            
            if (entry.ip_addr == ip_addr) {
                break;
            }
            
            if (entry.nud().weak) {
                last_weak_entry_ref = entry_ref;
            } else {
                num_hard++;
                last_hard_entry_ref = entry_ref;
            }
            
            entry_ref = m_used_entries_list.next(entry_ref, *this);
        }
        
        if (AIPSTACK_LIKELY(!entry_ref.isNull())) {
            // We found an entry with this IP address.
            // If this is a hard request, make sure the entry is hard.
            if (!weak) {
                (*entry_ref).nud().weak = false;
            }
        } else {
            // We did not find an entry with this IP address.
            // First do some checks of the IP address...
            
            // If this is the all-ones address, return the broadcast MAC address.
            if (ip_addr.isAllOnes()) {
                return GetArpEntryRes::BroadcastAddr;
            }
            
            // Check for zero IP address.
            if (ip_addr.isZero()) {
                return GetArpEntryRes::InvalidAddr;
            }
            
            // Check if the interface has an IP address assigned.
            IpIfaceIp4Addrs const *ifaddr = m_driver_iface.getIp4Addrs();
            if (ifaddr == nullptr) {
                return GetArpEntryRes::InvalidAddr;
            }
            
            // Check if the given IP address is in the subnet.
            if ((ip_addr & ifaddr->netmask) != ifaddr->netaddr) {
                return GetArpEntryRes::InvalidAddr;
            }
            
            // If this is the local broadcast address, return the broadcast MAC address.
            if (ip_addr == ifaddr->bcastaddr) {
                return GetArpEntryRes::BroadcastAddr;
            }
            
            // Check if there is a Free entry available.
            entry_ref = m_free_entries_list.first(*this);
            
            if (!entry_ref.isNull()) {
                // Got a Free entry.
                AIPSTACK_ASSERT((*entry_ref).nud().state == ArpEntryState::Free)
                AIPSTACK_ASSERT(!(*entry_ref).nud().timer_active)
                AIPSTACK_ASSERT(!(*entry_ref).retry_list.hasRequests())
                
                // Move the entry from the free list to the used list.
                m_free_entries_list.removeFirst(*this);
                m_used_entries_list.prepend(entry_ref, *this);
            } else {
                // There is no Free entry available, we will recycle a used entry.
                // Determine whether to recycle a weak or hard entry.
                bool use_weak;
                if (weak) {
                    use_weak =
                        !(num_hard > ArpProtectCount || last_weak_entry_ref.isNull());
                } else {
                    int num_weak = NumArpEntries - num_hard;
                    use_weak =
                        (num_weak > ArpNonProtectCount || last_hard_entry_ref.isNull());
                }
                
                // Get the entry to be recycled.
                entry_ref = use_weak ? last_weak_entry_ref : last_hard_entry_ref;
                AIPSTACK_ASSERT(!entry_ref.isNull())
                
                // Reset the entry, but keep it in the used list.
                reset_arp_entry(*entry_ref, true);
            }
            
            // NOTE: The entry is in Free state now but in the used list.
            // The caller is responsible to set a non-Free state ensuring
            // that the state corresponds with the list membership again.
            
            // Set IP address and weak flag.
            (*entry_ref).ip_addr = ip_addr;
            (*entry_ref).nud().weak = weak;
        }
        
        // Bump to entry to the front of the used entries list.
        if (!(entry_ref == m_used_entries_list.first(*this))) {
            m_used_entries_list.remove(entry_ref, *this);
            m_used_entries_list.prepend(entry_ref, *this);
        }
        
        // Return the entry.
        out_entry = entry_ref;
        return GetArpEntryRes::GotArpEntry;
    }
    
    // NOTE: update_timer is needed after this.
    void reset_arp_entry (ArpEntry &entry, bool leave_in_used_list)
    {
        AIPSTACK_ASSERT(entry.nud().state != ArpEntryState::Free)
        
        // Make sure the entry timeout is not active.
        clear_entry_timer(entry);
        
        // Set the entry to Free state.
        entry.nud().state = ArpEntryState::Free;
        
        // Reset the send-retry list for the entry.
        entry.retry_list.reset();
        
        // Move from used list to free list, unless requested not to.
        if (!leave_in_used_list) {
            m_used_entries_list.remove({entry, *this}, *this);
            m_free_entries_list.prepend({entry, *this}, *this);
        }
    }
    
    IpErr send_arp_packet (ArpOpType op_type, MacAddr dst_mac, Ip4Addr dst_ipaddr)
    {
        // Get a local buffer for the frame,
        TxAllocHelper<EthArpPktSize, HeaderBeforeEth> frame_alloc(EthArpPktSize);
        
        // Write the Ethernet header.
        auto eth_header = EthHeader::MakeRef(frame_alloc.getPtr());
        eth_header.set(EthHeader::DstMac(),  dst_mac);
        eth_header.set(EthHeader::SrcMac(),  *m_params.mac_addr);
        eth_header.set(EthHeader::EthType(), EthType::Arp);
        
        // Determine the source IP address.
        IpIfaceIp4Addrs const *ifaddr = m_driver_iface.getIp4Addrs();
        Ip4Addr src_addr = (ifaddr != nullptr) ? ifaddr->addr : Ip4Addr::ZeroAddr();
        
        // Write the ARP header.
        auto arp_header = ArpIp4Header::MakeRef(frame_alloc.getPtr() + EthHeader::Size);
        arp_header.set(ArpIp4Header::HwType(),       ArpHwType::Eth);
        arp_header.set(ArpIp4Header::ProtoType(),    EthType::Ipv4);
        arp_header.set(ArpIp4Header::HwAddrLen(),    MacAddr::Size);
        arp_header.set(ArpIp4Header::ProtoAddrLen(), Ip4Addr::Size);
        arp_header.set(ArpIp4Header::OpType(),       op_type);
        arp_header.set(ArpIp4Header::SrcHwAddr(),    *m_params.mac_addr);
        arp_header.set(ArpIp4Header::SrcProtoAddr(), src_addr);
        arp_header.set(ArpIp4Header::DstHwAddr(),    dst_mac);
        arp_header.set(ArpIp4Header::DstProtoAddr(), dst_ipaddr);
        
        // Send the frame via the lower-layer driver.
        return m_params.send_frame(frame_alloc.getBufRef());
    }
    
    // Set tne ARP entry timeout based on the entry state and attempts_left.
    void set_entry_timer (ArpEntry &entry)
    {
        AIPSTACK_ASSERT(!entry.nud().timer_active)
        AIPSTACK_ASSERT(entry.nud().state == one_of_timer_entry_states())
        AIPSTACK_ASSERT(entry.nud().state != ArpEntryState::Valid ||
                     entry.nud().attempts_left == 1)
        
        // Determine the relative timeout...
        TimeType timeout;
        if (entry.nud().state == ArpEntryState::Valid) {
            // Valid entry (not expired yet, i.e. with attempts_left==1).
            timeout = ArpValidTimeoutTicks;
        } else {
            // Query or Refreshing entry, compute timeout with exponential backoff.
            std::uint8_t attempts = (entry.nud().state == ArpEntryState::Query) ?
                ArpQueryAttempts : ArpRefreshAttempts;
            AIPSTACK_ASSERT(entry.nud().attempts_left <= attempts)
            timeout = ArpBaseResponseTimeoutTicks << (attempts - entry.nud().attempts_left);
        }
        
        // Get the current time.
        TimeType now = platform().getTime();
        
        // Update the reference time of the timer queue (needed before insert).
        m_timer_queue.updateReferenceTime(now, *this);
        
        // Calculate the absolute timeout time.
        TimeType abs_time = now + timeout;
        
        // Insert the timer to the timer queue and set the timer_active flag.
        m_timer_queue.insert({entry, *this}, abs_time, *this);
        entry.nud().timer_active = true;
    }
    
    // Make sure the entry timeout is not active.
    // NOTE: update_timer is needed after this.
    void clear_entry_timer (ArpEntry &entry)
    {
        // If the timer is active, remove it from the timer queue and clear the
        // timer_active flag.
        if (entry.nud().timer_active) {
            m_timer_queue.remove({entry, *this}, *this);
            entry.nud().timer_active = false;
        }
    }
    
    // Make sure the timer is set to expire at the first entry timeout,
    // or unset it if there is no active entry timeout. This must be called
    // after every insertion/removal of an entry to the timer queue.
    void update_timer ()
    {
        TimeType time;
        if (m_timer_queue.getFirstTime(time, *this)) {
            m_timer.setAt(time);
        } else {
            m_timer.unset();
        }
    }
    
    void timerHandler ()
    {
        // Prepare timer queue for removing expired timers.
        m_timer_queue.prepareForRemovingExpired(platform().getTime(), *this);
        
        // Dispatch expired timers...
        ArpEntryRef timer_ref;
        while (!(timer_ref = m_timer_queue.removeExpired(*this)).isNull()) {
            ArpEntry &entry = *timer_ref;
            AIPSTACK_ASSERT(entry.nud().timer_active)
            AIPSTACK_ASSERT(entry.nud().state == one_of_timer_entry_states())
            
            // Clear the timer_active flag since the entry has just been removed
            // from the timer queue.
            entry.nud().timer_active = false;
            
            // Perform timeout processing for the entry.
            handle_entry_timeout(entry);
        }
        
        // Set the timer for the next expiration or unset.
        update_timer();
    }
    
    void handle_entry_timeout (ArpEntry &entry)
    {
        AIPSTACK_ASSERT(entry.nud().state != ArpEntryState::Free)
        AIPSTACK_ASSERT(!entry.nud().timer_active)
        
        // Check if the IP address is still consistent with the interface
        // address settings. If not, reset the ARP entry.
        IpIfaceIp4Addrs const *ifaddr = m_driver_iface.getIp4Addrs();        
        if (ifaddr == nullptr ||
            (entry.ip_addr & ifaddr->netmask) != ifaddr->netaddr ||
            entry.ip_addr == ifaddr->bcastaddr)
        {
            reset_arp_entry(entry, false);
            return;
        }
        
        switch (entry.nud().state) {
            case ArpEntryState::Query: {
                // Query state: Decrement attempts_left then either reset the entry
                // in case of last attempt, else retransmit the broadcast query.
                AIPSTACK_ASSERT(entry.nud().attempts_left > 0)
                
                entry.nud().attempts_left--;
                if (entry.nud().attempts_left == 0) {
                    reset_arp_entry(entry, false);
                } else {
                    set_entry_timer(entry);
                    send_arp_packet(
                        ArpOpType::Request, MacAddr::BroadcastAddr(), entry.ip_addr);
                }
            } break;
            
            case ArpEntryState::Valid: {
                // Valid state: Set attempts_left to 0 to consider the entry expired.
                // Upon next use the entry it will go to Refreshing state.
                AIPSTACK_ASSERT(entry.nud().attempts_left == 1)
                
                entry.nud().attempts_left = 0;
            } break;
            
            case ArpEntryState::Refreshing: {
                // Refreshing state: Decrement attempts_left then either move
                // the entry to Query state (and send the first broadcast query),
                // else retransmit the unicast query.
                AIPSTACK_ASSERT(entry.nud().attempts_left > 0)
                
                entry.nud().attempts_left--;
                if (entry.nud().attempts_left == 0) {
                    entry.nud().state = ArpEntryState::Query;
                    entry.nud().attempts_left = ArpQueryAttempts;
                    send_arp_packet(
                        ArpOpType::Request, MacAddr::BroadcastAddr(), entry.ip_addr);
                } else {
                    send_arp_packet(ArpOpType::Request, entry.mac_addr, entry.ip_addr);
                }
                set_entry_timer(entry);
            } break;
            
            default:
                AIPSTACK_ASSERT(false);
        }
    }
    
private:
    EthIfaceDriverParams m_params;
    IpDriverIface<StackArg> m_driver_iface;
    typename Platform::Timer m_timer;
    EthArpObservable m_arp_observable;
    StructureRaiiWrapper<ArpEntryList> m_used_entries_list;
    StructureRaiiWrapper<ArpEntryList> m_free_entries_list;
    StructureRaiiWrapper<ArpEntryTimerQueue> m_timer_queue;
    TimeType m_timers_ref_time;
    EthHeader::Ref m_rx_eth_header;
    ArpEntry m_arp_entries[NumArpEntries];
    
    struct ArpEntriesAccessor :
        public MemberAccessor<EthIpIface, ArpEntry[NumArpEntries],
                              &EthIpIface::m_arp_entries> {};
};

/**
 * Static configuration options for @ref EthIpIface.
 */
struct EthIpIfaceOptions {
    /**
     * Size of the ARP cache (must be greater than zero).
     */
    AIPSTACK_OPTION_DECL_VALUE(NumArpEntries, int, 16)
    
    /**
     * Number of most-recently-used ARP cache entries to protect from reassignment.
     * 
     * This option is part of a mechanism to limit the reuse of existing ARP cache entries
     * for storing MAC addresses associated with IP addresses that are not actively being
     * resolved. This mechanism is especially relevent when the total number of hosts on
     * the subnet is greater than the ARP cache size. The exact algorithm is not described
     * and is subject to change.
     * 
     * Must be in the range [0, @ref NumArpEntries]; a good choice is
     * @ref NumArpEntries / 2.
     */
    AIPSTACK_OPTION_DECL_VALUE(ArpProtectCount, int, 8)
    
    /**
     * Header space to reserve in outgoing frames, for frames originating from
     * @ref EthIpIface itself.
     * 
     * If no additional headers will be added below the Ethernet header (the most likely
     * case), this should be 0.
     * 
     * See @ref EthIfaceDriverParams::send_frame for details.
     */
    AIPSTACK_OPTION_DECL_VALUE(HeaderBeforeEth, std::size_t, 0)
    
    /**
     * Data structure to use for ARP entry timers.
     * 
     * This should be one of the implementations in the folder aipstack/structure/minimum.
     * Specifically supported are @ref LinkedHeapService and @ref SortedListService.
     */
    AIPSTACK_OPTION_DECL_TYPE(TimersStructureService, void)
};

/**
 * Service definition for @ref EthIpIface.
 * 
 * The template parameters of this class are assignments of options defined in
 * @ref EthIpIfaceOptions, for example:
 * AIpStack::EthIpIfaceOptions::NumArpEntries::Is\<64\>.
 * 
 * An @ref EthIpIface class type can be obtained as follows:
 * 
 * ```
 * using MyEthIpIfaceService = AIpStack::EthIpIfaceService<...options...>;
 * class MyEthIpIfaceArg : public MyEthIpIfaceService::template Compose<
 *     PlatformImpl, IpStackArg> {};
 * using MyEthIpIface = AIpStack::EthIpIface<MyEthIpIfaceArg>;
 * ```
 * 
 * @tparam Options Assignments of options defined in @ref EthIpIfaceOptions.
 */
template <typename... Options>
class EthIpIfaceService {
    template <typename>
    friend class EthIpIface;
    
    AIPSTACK_OPTION_CONFIG_VALUE(EthIpIfaceOptions, NumArpEntries)
    AIPSTACK_OPTION_CONFIG_VALUE(EthIpIfaceOptions, ArpProtectCount)
    AIPSTACK_OPTION_CONFIG_VALUE(EthIpIfaceOptions, HeaderBeforeEth)
    AIPSTACK_OPTION_CONFIG_TYPE(EthIpIfaceOptions, TimersStructureService)
    
public:
    /**
     * Template to get the template parameter for @ref EthIpIface.
     * 
     * See @ref EthIpIfaceService for an example of instantiating the @ref EthIpIface.
     * It is advised to not pass this type directly to @ref EthIpIface but pass a dummy
     * user-defined class which inherits from it.
     * 
     * @tparam PlatformImpl_ Platform layer implementation, the same one as used by the
     *         @ref IpStack (see @ref IpStackService::Compose).
     * @tparam StackArg_ Template parameter of @ref IpStack.
     */
    template <typename PlatformImpl_, typename StackArg_>
    struct Compose {
#ifndef IN_DOXYGEN
        using PlatformImpl = PlatformImpl_;
        using StackArg = StackArg_;
        using Params = EthIpIfaceService;

        // This is for completeness and is not typically used.
        AIPSTACK_DEF_INSTANCE(Compose, EthIpIface)
#endif
    };
};

/** @} */

}

#endif
