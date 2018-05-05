/*
 * Copyright (c) 2018 Ambroz Bizjak
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

#ifndef AIPSTACK_IP_DRIVER_IFACE_H
#define AIPSTACK_IP_DRIVER_IFACE_H

#include <aipstack/misc/NonCopyable.h>
#include <aipstack/infra/Buf.h>
#include <aipstack/ip/IpAddr.h>
#include <aipstack/ip/IpStackTypes.h>
#include <aipstack/ip/IpIface.h>
#include <aipstack/ip/IpIfaceDriverParams.h>

namespace AIpStack {
#ifndef IN_DOXYGEN
    template <typename> class IpStack;
    template <typename> class IpIfaceStateObserver;
#endif
    
    /**
     * @addtogroup ip-stack
     * @{
     */
    
    /**
     * A logical network interface from the perspective of an IP-level interface
     * driver.
     * 
     * An interface driver creates an instance of this class to register a new
     * network interface with the stack, and destructs the instance to deregister
     * the interface.
     * 
     * @note Often the interface driver will not use this class directly but via
     * a class providing support for a lower-level protocol, such as @ref EthIpIface
     * for Ethernet/ARP support.
     * 
     * Each @ref IpDriverIface instance internally contains a single @ref IpIface
     * instance (accessible via @ref iface()), which represents the network interface
     * in a more general context. Interface drivers generally should not expose the
     * @ref IpDriverIface but should expose the @ref IpIface to allow the interface
     * to be configured and/or managed externally.
     * 
     * The interface driver and the stack interact in both directions:
     * - Calls from the driver to the stack are based on simple functions in @ref
     *   IpDriverIface (such as @ref recvIp4Packet to process received packets).
     * - Calls from the stack to the driver are based on polymorphic calls using
     *   @ref Function<Ret(Args...)> "Function" objects as provided by the driver
     *   ("driver functions") within the @ref IpIfaceDriverParams structure passed
     *   to the constructor (such as @ref IpIfaceDriverParams::send_ip4_packet to
     *   send a packet).
     * 
     * @tparam Arg Template parameter of @ref IpStack.
     */
    template <typename Arg>
    class IpDriverIface :
        private NonCopyable<IpDriverIface<Arg>>
#ifndef IN_DOXYGEN
        ,private IpIface<Arg>
#endif
    {
        template <typename> friend class IpIface;

    public:
        /**
         * Construct the network interface, registering it with the stack.
         * 
         * This should be used by the driver when the interface should start existing
         * from the perspective of the IP stack. After (and only after) this, the
         * various driver functions specified within @ref IpIfaceDriverParams may be
         * called.
         * 
         * The driver must be careful to not perform any action that might result in
         * calls of driver functions (such as sending packets to this interface) until
         * the driver is able to handle these calls.
         * 
         * @param stack Pointer to the IP stack.
         * @param params Interface parameters including driver functions, see @ref
         *        IpIfaceDriverParams. This structure is copied.
         */
        inline IpDriverIface (IpStack<Arg> *stack, IpIfaceDriverParams const &params) :
            IpIface<Arg>(stack, params)
        {}
        
        /**
         * Destruct the network interface, deregistering it from the stack.
         * 
         * The interface should be destructed when the interface should stop existing
         * from the perspective of the IP stack. After this, driver functions will
         * not be called any more, nor will any driver function be called from this
         * function.
         * 
         * The driver must be careful to not perform any action that might result in calls
         * of driver functions (such as sending packets to this interface) after it is
         * no longer ready to handle these calls.
         * 
         * When this is called, there must be no remaining @ref IpIfaceListener
         * objects listening on this interface or @ref IpIfaceStateObserver objects
         * observing this interface. Additionally, this must not be called in
         * potentially hazardous context with respect to IP processing, such as
         * from withing receive processing of this interface (@ref recvIp4Packet).
         * Safety can be ensured by performing the destruction from a top-level event
         * handler such as a timer.
         */
        inline ~IpDriverIface () = default;

        /**
         * Get the @ref IpIface representing this network interface.
         * 
         * @return Reference to the @ref IpIface.
         */
        inline IpIface<Arg> & iface () {
            return static_cast<IpIface<Arg> &>(*this);
        }

        /**
         * Process a received IPv4 packet.
         * 
         * This function should be called by the driver when an IPv4 packet is
         * received (or what appears to be one at least).
         * 
         * @note The driver must support various driver functions being called from
         * within this, especially @ref IpIfaceDriverParams::send_ip4_packet.
         * 
         * @param pkt Received packet, presumably starting with the IP header.
         *            The referenced buffers will only be read from within this
         *            function call.
         */
        inline void recvIp4Packet (IpBufRef pkt) {
            IpStack<Arg>::processRecvedIp4Packet(&iface(), pkt);
        }
        
        /**
         * Return information about the current IPv4 address assignment.
         * 
         * This can be used by the driver if it needs information about the
         * IPv4 address assigned to the interface, or other places where the
         * information is useful.
         * 
         * @return If no IPv4 address is assigned, then null. If an address is
         *         assigned, then a pointer to an @ref IpIfaceIp4Addrs structure
         *         with the adddress information. The pointer is only valid
         *         temporarily (it should not be cached).
         */
        inline IpIfaceIp4Addrs const * getIp4Addrs () {
            return iface().m_have_addr ? &iface().m_addr : nullptr;
        }
        
        /**
         * Notify that the driver-provided state may have changed.
         * 
         * This should be called by the driver after the values that would be
         * returned by @ref IpIfaceDriverParams::get_state have changed. It does
         * not strictly have to be called immediately after every change but it
         * should be called soon after a change.
         * 
         * @note The driver must support various driver functions being called from
         * within this, especially @ref IpIfaceDriverParams::send_ip4_packet.
         */
        void stateChanged ()
        {
            iface().m_state_observable.notifyKeepObservers(
                [&](IpIfaceStateObserver<Arg> &observer) {
                    observer.m_handler();
                });
        }
    };

    /** @} */
}

#endif
