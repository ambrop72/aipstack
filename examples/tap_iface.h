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

#include <aipstack/infra/Instance.h>
#include <aipstack/infra/Buf.h>
#include <aipstack/infra/Err.h>
#include <aipstack/platform/PlatformFacade.h>
#include <aipstack/platform/HostedPlatformImpl.h>
#include <aipstack/proto/EthernetProto.h>
#include <aipstack/eth/EthIpIface.h>

#ifdef _WIN32
#include "tap_windows/tap_windows.h"
#else
#include "tap_linux/tap_linux.h"
#endif

namespace AIpStackExamples {

template <typename StackArg, typename TheEthIpIfaceService>
class TapIface;

namespace Private {
    template <typename StackArg, typename TheEthIpIfaceService>
    class EthIpIfaceArg : public TheEthIpIfaceService::template Compose<
        AIpStack::HostedPlatformImpl, StackArg> {};
    
    struct TapIfaceMacAddr {
        AIpStack::MacAddr m_mac_addr;
    };
}

template <typename StackArg, typename TheEthIpIfaceService>
class TapIface :
    private TapDevice,
    private Private::TapIfaceMacAddr,
    public AIpStack::EthIpIface<Private::EthIpIfaceArg<StackArg, TheEthIpIfaceService>>
{
    using Platform = AIpStack::PlatformFacade<AIpStack::HostedPlatformImpl>;

    using TheEthIpIface =
        AIpStack::EthIpIface<Private::EthIpIfaceArg<StackArg, TheEthIpIfaceService>>;
    
public:
    TapIface (Platform platform, AIpStack::IpStack<StackArg> *stack,
              std::string const &device_id, AIpStack::MacAddr const &mac_addr)
    :
        TapDevice(platform.ref().platformImpl()->getEventLoop(), device_id),
        TapIfaceMacAddr{mac_addr},
        TheEthIpIface(platform, stack, {
            /*eth_mtu=*/ TapDevice::getMtu(),
            /*mac_addr=*/ &this->TapIfaceMacAddr::m_mac_addr
        })
    {}
    
private:
    // Implement TapDevice::frameReceived
    void frameReceived (AIpStack::IpBufRef frame) override final
    {
        return TheEthIpIface::recvFrameFromDriver(frame);
    }
    
    // Implement TheEthIpIface::driverSendFrame
    AIpStack::IpErr driverSendFrame (AIpStack::IpBufRef frame) override final
    {
        return TapDevice::sendFrame(frame);
    }
    
    // Implement TheEthIpIface::driverGetEthState
    AIpStack::EthIfaceState driverGetEthState () override final
    {
        AIpStack::EthIfaceState state = {};
        state.link_up = true;
        return state;
    }
};

}

#endif
