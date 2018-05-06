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

#ifndef AIPSTACK_IP_IFACE_DRIVER_PARAMS_H
#define AIPSTACK_IP_IFACE_DRIVER_PARAMS_H

#include <cstddef>

#include <aipstack/misc/Function.h>
#include <aipstack/infra/Err.h>
#include <aipstack/infra/Buf.h>
#include <aipstack/infra/SendRetry.h>
#include <aipstack/ip/IpAddr.h>
#include <aipstack/ip/IpStackTypes.h>
#include <aipstack/ip/hw/IpHwCommon.h>

namespace AIpStack {
    /**
     * @addtogroup ip-stack
     * @{
     */
    
    /**
     * Encapsulates interface parameters passed to the @ref IpDriverIface constructor.
     */
    struct IpIfaceDriverParams {
        /**
         * The Maximum Transmission Unit (MTU), including the IP header.
         * 
         * It must be at least @ref IpStack::MinMTU (this is an assert).
         */
        std::size_t ip_mtu = 0;
        
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

        /**
         * Driver function used to send an IPv4 packet through the interface.
         * 
         * @note This function must be provided.
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
        Function<IpErr(IpBufRef pkt, Ip4Addr ip_addr, IpSendRetryRequest *sendRetryReq)>
            send_ip4_packet = nullptr;
        
        /**
         * Driver function to get the driver-provided interface state.
         * 
         * @note This function must be provided.
         * 
         * The driver should call @ref IpDriverIface::stateChanged whenever the state
         * that would be returned here has changed.
         * 
         * @return Driver-provided-state (currently just the link-up flag).
         */
        Function<IpIfaceDriverState()> get_state = nullptr;
    };

    /** @} */
}

#endif
