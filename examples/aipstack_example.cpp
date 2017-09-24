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

#include <cstdio>
#include <memory>
#include <string>
#include <stdexcept>

#include <aipstack/proto/EthernetProto.h>
#include <aipstack/structure/index/AvlTreeIndex.h>
#include <aipstack/structure/index/MruListIndex.h>
#include <aipstack/structure/minimum/LinkedHeap.h>
#include <aipstack/ip/IpStack.h>
#include <aipstack/ip/IpPathMtuCache.h>
#include <aipstack/ip/IpReassembly.h>
#include <aipstack/tcp/IpTcpProto.h>
#include <aipstack/eth/EthIpIface.h>

#ifdef _WIN32
#include "tap_windows/tap_windows.h"
#else
#include "tap_linux/tap_linux.h"
#endif

#include "libuv_platform.h"
#include "libuv_app_helper.h"

// Address configuration
static AIpStack::MacAddr const DeviceMacAddr =
    AIpStack::MacAddr::Make(0x8e, 0x86, 0x90, 0x97, 0x65, 0xd5);
static AIpStack::Ip4Addr const DeviceIpAddr =
    AIpStack::Ip4Addr::FromBytes(192, 168, 64, 10);
static uint8_t const DevicePrefixLength = 24;
static AIpStack::Ip4Addr const DeviceGatewayAddr =
    AIpStack::Ip4Addr::FromBytes(192, 168, 64, 1);

static int const MaxConnections = 1024;

using PlatformImpl = AIpStackExamples::PlatformImplLibuv;

using IndexService = AIpStack::AvlTreeIndexService;
//using IndexService = AIpStack::MruListIndexService;

using MyIpStackService = AIpStack::IpStackService<
    AIpStack::IpStackOptions::HeaderBeforeIp::Is<AIpStack::EthHeader::Size>,
    AIpStack::IpStackOptions::PathMtuCacheService::Is<
        AIpStack::IpPathMtuCacheService<
            AIpStack::IpPathMtuCacheOptions::NumMtuEntries::Is<MaxConnections>,
            AIpStack::IpPathMtuCacheOptions::MtuIndexService::Is<
                IndexService
            >
        >
    >,
    AIpStack::IpStackOptions::ReassemblyService::Is<
        AIpStack::IpReassemblyService<
            AIpStack::IpReassemblyOptions::MaxReassEntrys::Is<16>,
            AIpStack::IpReassemblyOptions::MaxReassSize::Is<1480>
        >
    >
>;

using ProtocolServicesList = AIpStack::MakeTypeList<
    AIpStack::IpTcpProtoService<
        AIpStack::IpTcpProtoOptions::NumTcpPcbs::Is<MaxConnections>,
        AIpStack::IpTcpProtoOptions::PcbIndexService::Is<
            IndexService
        >
    >
>;

using MyEthIpIfaceService = AIpStack::EthIpIfaceService<
    AIpStack::EthIpIfaceOptions::NumArpEntries::Is<64>,
    AIpStack::EthIpIfaceOptions::ArpProtectCount::Is<32>,
    AIpStack::EthIpIfaceOptions::HeaderBeforeEth::Is<0>,
    AIpStack::EthIpIfaceOptions::TimersStructureService::Is<
        AIpStack::LinkedHeapService
    >
>;

using PlatformRef = AIpStack::PlatformRef<PlatformImpl>;
using Platform = AIpStack::PlatformFacade<PlatformImpl>;

AIPSTACK_MAKE_INSTANCE(MyIpStack, (MyIpStackService::template Compose<
    PlatformImpl, ProtocolServicesList>))

using Iface = typename MyIpStack::Iface;

AIPSTACK_MAKE_INSTANCE(MyEthIpIface, (MyEthIpIfaceService::template Compose<
    PlatformImpl, Iface>))

class TapIface :
    private AIpStackExamples::TapDevice,
    public MyEthIpIface
{
public:
    TapIface (::Platform platform, MyIpStack *stack, std::string const &device_id) :
        TapDevice(platform.ref().platformImpl()->loop(), device_id),
        MyEthIpIface(platform, stack, {
            /*eth_mtu=*/ TapDevice::getMtu(),
            /*mac_addr=*/ &DeviceMacAddr
        })
    {}
    
private:
    // Implement TapDevice::frameReceived
    void frameReceived (AIpStack::IpBufRef frame) override final
    {
        return MyEthIpIface::recvFrameFromDriver(frame);
    }
    
    // Implement MyEthIpIface::driverSendFrame
    AIpStack::IpErr driverSendFrame (AIpStack::IpBufRef frame) override final
    {
        return TapDevice::sendFrame(frame);
    }
    
    // Implement MyEthIpIface::driverGetEthState
    AIpStack::EthIfaceState driverGetEthState () override final
    {
        AIpStack::EthIfaceState state = {};
        state.link_up = true;
        return state;
    }
};

int main (int argc, char *argv[])
{
    std::string device_id = (argc > 1) ? argv[1] : "";
    
    AIpStackExamples::LibuvAppHelper app_helper;
    
    PlatformImpl platform_impl{app_helper.getLoop()};
    
    Platform platform{PlatformRef{&platform_impl}};
    
    std::unique_ptr<MyIpStack> stack{new MyIpStack(platform)};
    
    std::unique_ptr<TapIface> iface;
    try {
        iface.reset(new TapIface(platform, &*stack, device_id));
    }
    catch (std::runtime_error const &ex) {
        std::fprintf(stderr, "Error initializing TAP interface: %s\n",
                     ex.what());
        return 1;
    }
    
    iface->setIp4Addr({true, DevicePrefixLength, DeviceIpAddr});
    iface->setIp4Gateway({true, DeviceGatewayAddr});
    
    std::fprintf(stderr, "Initialized, entering event loop.\n");
    
    app_helper.run();
    
    return 0;
}
