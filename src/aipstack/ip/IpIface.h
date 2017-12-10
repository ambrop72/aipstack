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

#ifndef AIPSTACK_IP_IFACE_H
#define AIPSTACK_IP_IFACE_H

#include <stdint.h>

#include <aipstack/misc/MinMax.h>
#include <aipstack/misc/Assert.h>
#include <aipstack/misc/NonCopyable.h>
#include <aipstack/structure/LinkedList.h>
#include <aipstack/structure/StructureRaiiWrapper.h>
#include <aipstack/infra/Err.h>
#include <aipstack/infra/Buf.h>
#include <aipstack/infra/SendRetry.h>
#include <aipstack/infra/ObserverNotification.h>
#include <aipstack/proto/IpAddr.h>
#include <aipstack/ip/IpStackHelperTypes.h>
#include <aipstack/ip/hw/IpHwCommon.h>

namespace AIpStack {
    
#ifndef IN_DOXYGEN
    template <typename> class IpStack;
    template <typename> class IpIfaceListener;
    template <typename> class IpIfaceStateObserver;
#endif
    
    /**
     * @addtogroup ip-stack
     * @{
     */
    
    /**
     * A network interface.
     * 
     * This class is generally designed to be inherited and owned by the IP driver.
     * Virtual functions are used by the IP stack to request actions or information
     * from the driver (such as @ref driverSendIp4Packet to send a packet), while
     * protected non-virtual functions are to be used by the driver to request
     * service from the IP stack (such as @ref recvIp4PacketFromDriver to process
     * received packets).
     * 
     * The IP stack does not provide or impose any model for management of interfaces
     * and interface drivers. Such a system could be build on top if it is needed.
     * 
     * @tparam TheIpStack The @ref IpStack class type.
     */
    template <typename TheIpStack>
    class IpIface :
        private NonCopyable<IpIface<TheIpStack>>
    {
        template <typename> friend class IpStack;
        template <typename> friend class IpIfaceListener;
        template <typename> friend class IpIfaceStateObserver;

    public:
        /**
         * The @ref AIpStack::IpStack "IpStack" type that this class is associated with.
         */
        using IfaceIpStack = TheIpStack;
        
        /**
         * Construct the interface.
         * 
         * This should be used by the driver when the interface should start existing
         * from the perspective of the IP stack. After this, the various virtual
         * functions may be called.
         * 
         * The owner must be careful to not perform any action that might result in calls
         * of virtual functions (such as sending packets to this interface) until the
         * derived class is constructed and ready to accept these calls.
         * 
         * @param stack Pointer to the IP stack.
         * @param info Interface information, see @ref IpIfaceInitInfo.
         */
        IpIface (TheIpStack *stack, IpIfaceInitInfo const &info) :
            m_stack(stack),
            m_hw_iface(info.hw_iface),
            m_ip_mtu(MinValueU(TypeMax<uint16_t>(), info.ip_mtu)),
            m_hw_type(info.hw_type),
            m_have_addr(false),
            m_have_gateway(false)
        {
            AIPSTACK_ASSERT(stack != nullptr)
            AIPSTACK_ASSERT(m_ip_mtu >= TheIpStack::MinMTU)
            
            // Register interface.
            m_stack->m_iface_list.prepend(*this);
        }
        
        /**
         * Destruct the interface.
         * 
         * The interface should be destructed when the interface should stop existing
         * from the perspective of the IP stack. After this, virtual functions will
         * not be called any more, nor will any virtual function be called from this
         * function.
         * 
         * The owner must be careful to not perform any action that might result in calls
         * of virtual functions (such as sending packets to this interface) after
         * destruction of the derived class has begun or generally after it is no longer
         * ready to accept these calls.
         * 
         * When this is called, there must be no remaining @ref IpIfaceListener
         * objects listening on this interface or @ref IpIfaceStateObserver objects
         * observing this interface. Additionally, this must not be called in
         * potentially hazardous context with respect to IP processing, such as
         * from withing receive processing of this interface
         * (@ref recvIp4PacketFromDriver). For maximum safety this should be called
         * from a top-level event handler.
         */
        ~IpIface ()
        {
            AIPSTACK_ASSERT(m_listeners_list.isEmpty())
            
            // Unregister interface.
            m_stack->m_iface_list.remove(*this);
        }
        
        /**
         * Set or remove the IP address and subnet prefix length.
         * 
         * @param value New IP address settings. If the "present" field is false
         *        then any existing assignment is removed. If the "present" field
         *        is true then the IP address in the "addr" field is assigned along
         *        with the subnet prefix length in the "prefix" field, overriding
         *        any existing assignment.
         */
        void setIp4Addr (IpIfaceIp4AddrSetting value)
        {
            AIPSTACK_ASSERT(!value.present || value.prefix <= Ip4Addr::Bits)
            
            m_have_addr = value.present;
            if (value.present) {
                m_addr.addr = value.addr;
                m_addr.netmask = Ip4Addr::PrefixMask(value.prefix);
                m_addr.netaddr = m_addr.addr & m_addr.netmask;
                m_addr.bcastaddr = m_addr.netaddr |
                    (Ip4Addr::AllOnesAddr() & ~m_addr.netmask);
                m_addr.prefix = value.prefix;
            }
        }
        
        /**
         * Get the current IP address settings.
         * 
         * @return Current IP address settings. If the "present" field is false
         *         then no IP address is assigned, and the "addr" and "prefix" fields
         *         will be zero. If the "present" field is true then the "addr" and
         *         "prefix" fields contain the assigned address and subnet prefix
         *         length.
         */
        IpIfaceIp4AddrSetting getIp4Addr ()
        {
            IpIfaceIp4AddrSetting value = {m_have_addr};
            if (m_have_addr) {
                value.prefix = m_addr.prefix;
                value.addr = m_addr.addr;
            }
            return value;
        }
        
        /**
         * Set or remove the gateway address.
         * 
         * @param value New gateway address settings. If the "present" field is false
         *        then any existing gateway address is removed. If the "present" field
         *        is true then gateway address in the "addr" field is assigned,
         *        overriding any existing assignment.
         */
        void setIp4Gateway (IpIfaceIp4GatewaySetting value)
        {
            m_have_gateway = value.present;
            if (value.present) {
                m_gateway = value.addr;
            }
        }
        
        /**
         * Get the current gateway address settings.
         * 
         * @return Current gateway address settings. If the "present" field is false
         *         then no gateway address is assigned, and the "addr" field will be
         *         zero. If the "present" field is true then the "addr" field contains
         *         the assigned gateway address.
         */
        IpIfaceIp4GatewaySetting getIp4Gateway ()
        {
            IpIfaceIp4GatewaySetting value = {m_have_gateway};
            if (m_have_gateway) {
                value.addr = m_gateway;
            }
            return value;
        }
        
        /**
         * Get the type of the hardware-type-specific interface.
         * 
         * The can be used to check which kind of hardware-type-specific interface
         * is available via @ref getHwIface (if any). For example, if the result is
         * @ref IpHwType::Ethernet, then an interface of type @ref EthHwIface
         * is available.
         * 
         * This function will return whatever was passed as @ref IpIfaceInitInfo::hw_type
         * when the interface was constructed.
         * 
         * This mechanism was created to support the DHCP client which requires
         * access to certain Ethernet/ARP-level functionality.
         * 
         * @return Type of hardware-type-specific interface.
         */
        inline IpHwType getHwType ()
        {
            return m_hw_type;
        }
        
        /**
         * Get the hardware-type-specific interface.
         * 
         * The HwIface type must correspond to the value returned by @ref getHwType.
         * If that value is @ref IpHwType::Undefined, then this function should not
         * be called at all.
         * 
         * This function will return whatever was passed as @ref IpIfaceInitInfo::hw_iface
         * when the interface was constructed.
         * 
         * @tparam HwIface Type of hardware-type-specific interface.
         * @return Pointer to hardware-type-specific interface.
         */
        template <typename HwIface>
        inline HwIface * getHwIface ()
        {
            return static_cast<HwIface *>(m_hw_iface);
        }
        
    public:
        /**
         * Check if an address belongs to the subnet of the interface.
         * 
         * @param addr Address to check.
         * @return True if the interface has an IP address assigned and the
         *         given address belongs to the associated subnet, false otherwise.
         */
        inline bool ip4AddrIsLocal (Ip4Addr addr)
        {
            return m_have_addr && (addr & m_addr.netmask) == m_addr.netaddr;
        }
        
        /**
         * Check if an address is the local broadcast address of the interface.
         * 
         * @param addr Address to check.
         * @return True if the interface has an IP address assigned and the
         *         given address is the associated local broadcast address,
         *         false otherwise.
         */
        inline bool ip4AddrIsLocalBcast (Ip4Addr addr)
        {
            return m_have_addr && addr == m_addr.bcastaddr;
        }
        
        /**
         * Check if an address is the address of the interface.
         * 
         * @param addr Address to check.
         * @return True if the interface has an IP address assigned and the
         *         assigned address is the given address, false otherwise.
         */
        inline bool ip4AddrIsLocalAddr (Ip4Addr addr)
        {
            return m_have_addr && addr == m_addr.addr;
        }
        
        /**
         * Return the IP level Maximum Transmission Unit of the interface.
         * 
         * @return MTU in bytes including the IP header. It will be at least
         *         @ref IpStack::MinMTU.
         */
        inline uint16_t getMtu ()
        {
            return m_ip_mtu;
        }
        
        /**
         * Return the driver-provided interface state.
         * 
         * This directly queries the driver for the current state by calling the
         * virtual function @ref driverGetState. Use @ref IpIfaceStateObserver if
         * you need to be notified of changes of this state.
         * 
         * Currently, the driver-provided state indicates whether the link is up.
         * 
         * @return Driver-provided state.
         */
        inline IpIfaceDriverState getDriverState ()
        {
            return driverGetState();
        }
        
    protected:
        /**
         * Driver function used to send an IPv4 packet through the interface.
         * 
         * This is called whenever an IPv4 packet needs to be sent. The driver should
         * copy the packet as needed because it must not access the referenced buffers
         * outside this function.
         * 
         * @param pkt Packet to send, this includes the IP header. It is guaranteed
         *        that its size does not exceed the MTU reported by the driver. The
         *        packet is expected to have HeaderBeforeIp bytes available before
         *        the IP header for link-layer protocol headers, but needed header
         *        space should still be checked since higher-layer prococols are
         *        responsible for allocating the buffers of packets they send.
         * @param ip_addr Next hop address.
         * @param sendRetryReq If sending fails and this is not null, the driver
         *        may use this to notify the requestor when sending should be retried.
         *        For example if the issue was that there is no ARP cache entry
         *        or similar entry for the given address, the notification should
         *        be done when the associated ARP query is successful.
         * @return Success or error code.
         */
        virtual IpErr driverSendIp4Packet (IpBufRef pkt, Ip4Addr ip_addr,
                                           IpSendRetryRequest *sendRetryReq) = 0;
        
        /**
         * Driver function to get the driver-provided interface state.
         * 
         * The driver should call @ref stateChangedFromDriver whenever the state
         * that would be returned here has changed.
         * 
         * @return Driver-provided-state (currently just the link-up flag).
         */
        virtual IpIfaceDriverState driverGetState () = 0;
        
        /**
         * Process a received IPv4 packet.
         * 
         * This function should be called by the driver when an IPv4 packet is
         * received (or what appears to be one at least).
         * 
         * The driver must support various driver functions being called from
         * within this, especially @ref driverSendIp4Packet.
         * 
         * @param pkt Received packet, presumably starting with the IP header.
         *            The referenced buffers will only be read from within this
         *            function call.
         */
        inline void recvIp4PacketFromDriver (IpBufRef pkt)
        {
            TheIpStack::processRecvedIp4Packet(this, pkt);
        }
        
        /**
         * Return information about current IPv4 address assignment.
         * 
         * This can be used by the driver if it needs information about the
         * IPv4 address assigned to the interface, or other places where the
         * information is useful.
         * 
         * @return If no IPv4 address is assigned, then null. If an address is
         *         assigned, then a pointer to a structure providing information
         *         about the assignment (assigned address, network mask, network
         *         address, broadcast address, subnet prefix length). The pointer
         *         is only valid temporarily (it should not be cached).
         */
        inline IpIfaceIp4Addrs const * getIp4AddrsFromDriver ()
        {
            return m_have_addr ? &m_addr : nullptr;
        }
        
        /**
         * Notify that the driver-provided state may have changed.
         * 
         * This should be called by the driver after the values that would be
         * returned by @ref driverGetState have changed. It does not strictly have
         * to be called immediately after every change but it should be called
         * soon after a change.
         * 
         * The driver must support various driver functions being called from
         * within this, especially @ref driverSendIp4Packet.
         */
        void stateChangedFromDriver ()
        {
            m_state_observable.notifyKeepObservers([&](
                typename TheIpStack::IfaceStateObserver &observer)
            {
                observer.ifaceStateChanged();
            });
        }
        
    private:
        LinkedListNode<typename TheIpStack::IfaceLinkModel> m_iface_list_node;
        StructureRaiiWrapper<typename TheIpStack::IfaceListenerList> m_listeners_list;
        Observable<typename TheIpStack::IfaceStateObserver> m_state_observable;
        TheIpStack *m_stack;
        void *m_hw_iface;
        uint16_t m_ip_mtu;
        IpIfaceIp4Addrs m_addr;
        Ip4Addr m_gateway;
        IpHwType m_hw_type;
        bool m_have_addr;
        bool m_have_gateway;
    };
    
    /** @} */
}

#endif
