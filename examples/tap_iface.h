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

#ifndef AIPSTACK_TAP_IFACE_H
#define AIPSTACK_TAP_IFACE_H

#include <string>

#include <aipstack/misc/Function.h>
#include <aipstack/infra/Instance.h>
#include <aipstack/infra/Buf.h>
#include <aipstack/infra/Err.h>
#include <aipstack/platform/PlatformFacade.h>
#include <aipstack/platform/HostedPlatformImpl.h>
#include <aipstack/eth/EthIpIface.h>
#include <aipstack/eth/MacAddr.h>
#include <aipstack/tap/TapDevice.h>

namespace AIpStackExamples {

template <typename StackArg, typename TheEthIpIfaceService>
class TapIface {
    using Platform = AIpStack::PlatformFacade<AIpStack::HostedPlatformImpl>;

    AIPSTACK_MAKE_INSTANCE(TheEthIpIface, (TheEthIpIfaceService::template Compose<
        AIpStack::HostedPlatformImpl, StackArg>))

public:
    TapIface (Platform platform, AIpStack::IpStack<StackArg> *stack,
              std::string const &device_id, AIpStack::MacAddr const &mac_addr)
    :
        m_tap_device(platform.ref().platformImpl()->getEventLoop(), device_id,
            AIPSTACK_BIND_MEMBER_TN(&TapIface::frameReceived, this)),
        m_mac_addr(mac_addr),
        m_eth_iface(platform, stack, AIpStack::EthIfaceDriverParams{
            /*eth_mtu=*/ m_tap_device.getMtu(),
            /*mac_addr=*/ &m_mac_addr,
            AIPSTACK_BIND_MEMBER_TN(&TapIface::driverSendFrame, this),
            AIPSTACK_BIND_MEMBER_TN(&TapIface::driverGetEthState, this)
        })
    {}

    inline AIpStack::IpIface<StackArg> & iface () {
        return m_eth_iface.iface();
    }
    
private:
    void frameReceived (AIpStack::IpBufRef frame)
    {
        return m_eth_iface.recvFrame(frame);
    }
    
    AIpStack::IpErr driverSendFrame (AIpStack::IpBufRef frame)
    {
        return m_tap_device.sendFrame(frame);
    }
    
    AIpStack::EthIfaceState driverGetEthState ()
    {
        AIpStack::EthIfaceState state = {};
        state.link_up = true;
        return state;
    }

private:
    AIpStack::TapDevice m_tap_device;
    AIpStack::MacAddr m_mac_addr;
    TheEthIpIface m_eth_iface;
};

}

#endif
