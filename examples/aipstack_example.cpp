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

#include <aipstack/proto/IpAddr.h>
#include <aipstack/proto/EthernetProto.h>
#include <aipstack/structure/index/AvlTreeIndex.h>
#include <aipstack/structure/index/MruListIndex.h>
#include <aipstack/structure/minimum/LinkedHeap.h>
#include <aipstack/platform/PlatformFacade.h>
#include <aipstack/ip/IpStack.h>
#include <aipstack/ip/IpPathMtuCache.h>
#include <aipstack/ip/IpReassembly.h>
#include <aipstack/ip/IpDhcpClient.h>
#include <aipstack/ip/IpProtocolHandlerStub.h>
#include <aipstack/tcp/IpTcpProto.h>
#include <aipstack/udp/IpUdpProto.h>
#include <aipstack/eth/EthIpIface.h>

#include "libuv_platform.h"
#include "libuv_app_helper.h"
#include "tap_iface.h"
#include "example_server.h"


// CONFIGURATION

// Address configuration
static bool const DeviceUseDhcp = true;
static AIpStack::Ip4Addr const DeviceIpAddr =
    AIpStack::Ip4Addr::FromBytes(192, 168, 64, 10);
static uint8_t const DevicePrefixLength = 24;
static AIpStack::Ip4Addr const DeviceGatewayAddr =
    AIpStack::Ip4Addr::FromBytes(192, 168, 64, 1);
static AIpStack::MacAddr const DeviceMacAddr =
    AIpStack::MacAddr::Make(0x8e, 0x86, 0x90, 0x97, 0x65, 0xd5);

// Index data structure to use for various things.
using IndexService = AIpStack::AvlTreeIndexService; // AVL tree
//using IndexService = AIpStack::MruListIndexService; // Linked list

// IP layer (IpStack) configuration
using MyIpStackService = AIpStack::IpStackService<
    AIpStack::IpStackOptions::HeaderBeforeIp::Is<AIpStack::EthHeader::Size>,
    AIpStack::IpStackOptions::PathMtuCacheService::Is<
        AIpStack::IpPathMtuCacheService<
            AIpStack::IpPathMtuCacheOptions::NumMtuEntries::Is<512>,
            AIpStack::IpPathMtuCacheOptions::MtuIndexService::Is<
                IndexService
            >
        >
    >,
    AIpStack::IpStackOptions::ReassemblyService::Is<
        AIpStack::IpReassemblyService<
            AIpStack::IpReassemblyOptions::MaxReassEntrys::Is<16>,
            AIpStack::IpReassemblyOptions::MaxReassSize::Is<60000>
        >
    >
>;

// List of transport protocols
using ProtocolServicesList = AIpStack::MakeTypeList<
    // TCP configuration
    AIpStack::IpTcpProtoService<
        AIpStack::IpTcpProtoOptions::NumTcpPcbs::Is<2048>,
        AIpStack::IpTcpProtoOptions::PcbIndexService::Is<IndexService>
    >,
    // UDP configuration
    AIpStack::IpUdpProtoService<
        AIpStack::IpUdpProtoOptions::UdpIndexService::Is<IndexService>
    >
    // Uncomment to test that IpProtocolHandlerStub compiles
    //,AIpStack::IpProtocolHandlerStubService
>;

// Ethernet layer (EthIpIface) configuration
using MyEthIpIfaceService = AIpStack::EthIpIfaceService<
    AIpStack::EthIpIfaceOptions::NumArpEntries::Is<64>,
    AIpStack::EthIpIfaceOptions::ArpProtectCount::Is<32>,
    AIpStack::EthIpIfaceOptions::HeaderBeforeEth::Is<0>,
    AIpStack::EthIpIfaceOptions::TimersStructureService::Is<
        AIpStack::LinkedHeapService
    >
>;

// DHCP client (IpDhcpClient) configuration.
using MyDhcpClientService = AIpStack::IpDhcpClientService<
    // use defaults    
>;

// Example server configuration.
using MyExampleServerService = AIpStackExamples::ExampleServerService<
    // use defaults
>;

// CONFIGURATION - END


// Type alias for our platform implementation class.
using PlatformImpl = AIpStackExamples::PlatformImplLibuv;

// Type aliases for PlatformRef and PlatformFacade.
using PlatformRef = AIpStack::PlatformRef<PlatformImpl>;
using Platform = AIpStack::PlatformFacade<PlatformImpl>;

// Instantiate the IpStack.
class IpStackArg : public MyIpStackService::template Compose<
    PlatformImpl, ProtocolServicesList> {};
using MyIpStack = AIpStack::IpStack<IpStackArg>;

// Instantiate the TapIface.
using MyTapIface = AIpStackExamples::TapIface<IpStackArg, MyEthIpIfaceService>;

// Instantiate the IpDhcpClient.
class DhcpClientArg : public MyDhcpClientService::template Compose<
    PlatformImpl, IpStackArg> {};
using MyDhcpClient = AIpStack::IpDhcpClient<DhcpClientArg>;

// Instantiate the example server.
class MyExampleServerArg : public MyExampleServerService::template Compose<IpStackArg> {};


int main (int argc, char *argv[])
{
    std::string device_id = (argc > 1) ? argv[1] : "";
    
    // Construct the LibuvAppHelper (manages the event loop etc).
    AIpStackExamples::LibuvAppHelper app_helper;
    
    // Construct the platform implementation class instance.
    PlatformImpl platform_impl{app_helper.getLoop()};
    
    // Construct a PlatformFacade instance for passing to various constructors.
    // This is a value-like class just referencing the platform_impl, it has no
    // resources of its own.
    Platform platform{PlatformRef{&platform_impl}};
    
    // Construct the IP stack.
    auto stack = std::make_unique<MyIpStack>(platform);
    
    // Construct the TAP interface.
    std::unique_ptr<MyTapIface> iface;
    try {
        iface = std::make_unique<MyTapIface>(platform, &*stack, device_id, DeviceMacAddr);
    }
    catch (std::runtime_error const &ex) {
        std::fprintf(stderr, "Error initializing TAP interface: %s\n",
                     ex.what());
        return 1;
    }
    
    std::unique_ptr<MyDhcpClient> dhcp_client;
    
    if (DeviceUseDhcp) {
        // Construct the DHCP client.
        AIpStack::IpDhcpClientInitOptions dhcp_opts;
        dhcp_client = std::make_unique<MyDhcpClient>(
            platform, &*stack, &*iface, dhcp_opts, nullptr);
    } else {
        // Assign static IP configuration.
        iface->setIp4Addr(
            AIpStack::IpIfaceIp4AddrSetting(DevicePrefixLength, DeviceIpAddr));
        iface->setIp4Gateway(AIpStack::IpIfaceIp4GatewaySetting(DeviceGatewayAddr));
    }
    
    // Construct the example server.
    auto example_server = 
        std::make_unique<AIpStackExamples::ExampleServer<MyExampleServerArg>>(&*stack);
    
    std::fprintf(stderr, "Initialized, entering event loop.\n");
    
    // Run the event loop.
    app_helper.run();
    
    return 0;
}
