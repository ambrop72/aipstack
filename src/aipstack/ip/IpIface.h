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

#include <cstdint>

#include <aipstack/misc/MinMax.h>
#include <aipstack/misc/Assert.h>
#include <aipstack/misc/NonCopyable.h>
#include <aipstack/structure/LinkedList.h>
#include <aipstack/structure/StructureRaiiWrapper.h>
#include <aipstack/infra/ObserverNotification.h>
#include <aipstack/ip/IpAddr.h>
#include <aipstack/ip/IpStackTypes.h>
#include <aipstack/ip/IpIfaceDriverParams.h>
#include <aipstack/ip/IpHwCommon.h>

namespace AIpStack {

#ifndef IN_DOXYGEN
template <typename> class IpStack;
template <typename> class IpIfaceListener;
template <typename> class IpIfaceStateObserver;
template <typename> class IpDriverIface;
#endif

/**
 * @addtogroup ip-stack
 * @{
 */

/**
 * A logical network interface in general context.
 * 
 * Instances of this class cannot be constructed stand-alone but are always
 * constructed implicitly as part of @ref IpDriverIface; the @ref IpIface for a
 * given @ref IpDriverIface can be retrieved using @ref IpDriverIface::iface.
 * 
 * @warning It is generally unsafe to store pointers to @ref IpIface, because
 * they would be invalidated when the driver removes the network interface by
 * destructing the associated @ref IpDriverIface object. There is currently no
 * mechanism to detect when a network interface has been removed.
 * 
 * The IP stack does not provide or impose any model for management of interfaces
 * and interface drivers. Such a system could be build on top if it is needed.
 * 
 * @tparam Arg Template parameter of @ref IpStack.
 */
template <typename Arg>
class IpIface :
    private NonCopyable<IpIface<Arg>>
{
    template <typename> friend class IpStack;
    template <typename> friend class IpIfaceListener;
    template <typename> friend class IpIfaceStateObserver;
    template <typename> friend class IpDriverIface;

private:
    IpIface (IpStack<Arg> *stack, IpIfaceDriverParams const &params) :
        m_stack(stack),
        m_params(params),
        m_ip_mtu(MinValueU(TypeMax<std::uint16_t>, params.ip_mtu)),
        m_have_addr(false),
        m_have_gateway(false)
    {
        AIPSTACK_ASSERT(stack != nullptr)
        AIPSTACK_ASSERT(m_ip_mtu >= IpStack<Arg>::MinMTU)
        AIPSTACK_ASSERT(params.send_ip4_packet)
        AIPSTACK_ASSERT(params.get_state)
        
        // Add the interface to the list of interfaces.
        m_stack->m_iface_list.prepend(*this);
    }

    ~IpIface ()
    {
        AIPSTACK_ASSERT(m_listeners_list.isEmpty())
        
        // Remove the interface from the list of interfaces.
        m_stack->m_iface_list.remove(*this);
    }

    inline IpDriverIface<Arg> & driver () {
        return static_cast<IpDriverIface<Arg> &>(*this);
    }

public:
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
    IpIfaceIp4AddrSetting getIp4Addr () const
    {
        return m_have_addr ?
            IpIfaceIp4AddrSetting(m_addr.prefix, m_addr.addr) :
            IpIfaceIp4AddrSetting();
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
    IpIfaceIp4GatewaySetting getIp4Gateway () const
    {
        return m_have_gateway ?
            IpIfaceIp4GatewaySetting(m_gateway) : IpIfaceIp4GatewaySetting();
    }
    
    /**
     * Get the type of the hardware-type-specific interface.
     * 
     * The can be used to check which kind of hardware-type-specific interface
     * is available via @ref getHwIface (if any). For example, if the result is
     * @ref IpHwType::Ethernet, then an interface of type @ref EthHwIface
     * is available.
     * 
     * This function will return whatever was passed as @ref
     * IpIfaceDriverParams::hw_type when the interface was added.
     * 
     * This mechanism was created to support the DHCP client which requires
     * access to certain Ethernet/ARP-level functionality.
     * 
     * @return Type of hardware-type-specific interface.
     */
    inline IpHwType getHwType () const {
        return m_params.hw_type;
    }
    
    /**
     * Get the hardware-type-specific interface.
     * 
     * The HwIface type must correspond to the value returned by @ref getHwType.
     * If that value is @ref IpHwType::Undefined, then this function should not
     * be called at all.
     * 
     * This function will return whatever was passed as @ref
     * IpIfaceDriverParams::hw_iface when the interface was added.
     * 
     * @tparam HwIface Type of hardware-type-specific interface.
     * @return Pointer to hardware-type-specific interface.
     */
    template <typename HwIface>
    inline HwIface * getHwIface () {
        return static_cast<HwIface *>(m_params.hw_iface);
    }
    
    /**
     * Check if an address belongs to the subnet of the interface.
     * 
     * @param addr Address to check.
     * @return True if the interface has an IP address assigned and the
     *         given address belongs to the associated subnet, false otherwise.
     */
    inline bool ip4AddrIsLocal (Ip4Addr addr) const {
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
    inline bool ip4AddrIsLocalBcast (Ip4Addr addr) const {
        return m_have_addr && addr == m_addr.bcastaddr;
    }
    
    /**
     * Check if an address is the address of the interface.
     * 
     * @param addr Address to check.
     * @return True if the interface has an IP address assigned and the
     *         assigned address is the given address, false otherwise.
     */
    inline bool ip4AddrIsLocalAddr (Ip4Addr addr) const {
        return m_have_addr && addr == m_addr.addr;
    }
    
    /**
     * Return the IP level Maximum Transmission Unit of the interface.
     * 
     * @return MTU in bytes including the IP header. It will be at least
     *         @ref IpStack::MinMTU.
     */
    inline std::uint16_t getMtu () const {
        return m_ip_mtu;
    }
    
    /**
     * Return the driver-provided interface state.
     * 
     * This directly queries the driver for the current state by calling the
     * driver function @ref IpIfaceDriverParams::get_state. Use @ref
     * IpIfaceStateObserver if you need to be notified of changes of this state.
     * 
     * Currently, the driver-provided state indicates whether the link is up.
     * 
     * @return Driver-provided state.
     */
    inline IpIfaceDriverState getDriverState () const {
        return m_params.get_state();
    }

private:
    LinkedListNode<typename IpStack<Arg>::IfaceLinkModel> m_iface_list_node;
    StructureRaiiWrapper<typename IpStack<Arg>::IfaceListenerList> m_listeners_list;
    Observable<IpIfaceStateObserver<Arg>> m_state_observable;
    IpStack<Arg> *m_stack;
    IpIfaceDriverParams m_params;
    std::uint16_t m_ip_mtu;
    IpIfaceIp4Addrs m_addr;
    Ip4Addr m_gateway;
    bool m_have_addr;
    bool m_have_gateway;
};

/** @} */

}

#endif
