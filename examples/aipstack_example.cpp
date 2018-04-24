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
#include <cstring>
#include <memory>
#include <string>
#include <stdexcept>

#include <aipstack/misc/Function.h>
#include <aipstack/proto/EthernetProto.h>
#include <aipstack/structure/index/AvlTreeIndex.h>
#include <aipstack/structure/index/MruListIndex.h>
#include <aipstack/structure/minimum/LinkedHeap.h>
#include <aipstack/platform/PlatformFacade.h>
#include <aipstack/platform/HostedPlatformImpl.h>
#include <aipstack/event_loop/EventLoop.h>
#include <aipstack/event_loop/SignalWatcher.h>
#include <aipstack/ip/IpAddr.h>
#include <aipstack/ip/IpStack.h>
#include <aipstack/ip/IpPathMtuCache.h>
#include <aipstack/ip/IpReassembly.h>
#include <aipstack/ip/IpDhcpClient.h>
#include <aipstack/ip/IpProtocolHandlerStub.h>
#include <aipstack/tcp/IpTcpProto.h>
#include <aipstack/udp/IpUdpProto.h>
#include <aipstack/eth/EthIpIface.h>
#include <aipstack/utils/IpAddrFormat.h>

#include "tap_iface.h"
#include "example_app.h"

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

// Example application configuration.
using MyExampleAppService = AIpStackExamples::ExampleAppService<
    // use defaults
>;

// CONFIGURATION - END


// Type alias for our platform implementation class.
using PlatformImpl = AIpStack::HostedPlatformImpl;

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

// Instantiate the example application.
class MyExampleAppArg : public MyExampleAppService::template Compose<IpStackArg> {};
using MyExampleApp = AIpStackExamples::ExampleApp<MyExampleAppArg>;

// Callback function for printing DHCP client events
static void dhcpClientCallback (
    std::unique_ptr<MyDhcpClient> const &dhcp, AIpStack::IpDhcpClientEvent event_type)
{
    char buf[30 + AIpStack::MaxIp4AddrPrintLen];
    char const *event_str = nullptr;

    switch (event_type) {
        case AIpStack::IpDhcpClientEvent::LeaseObtained:
            std::strcpy(buf, "Lease obtained: ");
            AIpStack::FormatIpAddr(buf + std::strlen(buf),
                dhcp->getLeaseInfoMustHaveLease().ip_address);
            event_str = buf;
            break;
        case AIpStack::IpDhcpClientEvent::LeaseRenewed:
            event_str = "Lease renewed";
            break;
        case AIpStack::IpDhcpClientEvent::LeaseLost:
            event_str = "Lease lost";
            break;
        case AIpStack::IpDhcpClientEvent::LinkDown:
            event_str = "Link down";
            break;
        default:
            break;
    }

    if (event_str != nullptr) {
        std::printf("DHCP: %s\n", event_str);
    }
}

int main (int argc, char *argv[])
{
    std::string device_id = (argc > 1) ? argv[1] : "";
    
    // Construct the SignalCollector.
    AIpStack::SignalCollector signal_collector(AIpStack::SignalType::ExitSignals);

    // Construct the event loop.
    AIpStack::EventLoop event_loop;

    // Construct the SignalWatcher.
    AIpStack::SignalWatcher signal_watcher(event_loop, signal_collector,
    [&event_loop](AIpStack::SignalInfo signal_info) {
        std::printf("Got signal %s, terminating...\n",
            nativeNameForSignalType(signal_info.type));
        event_loop.stop();
    });
    
    // Construct the platform implementation class instance.
    PlatformImpl platform_impl{event_loop};
    
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
            platform, &*stack, &iface->iface(), dhcp_opts,
            [&dhcp_client](AIpStack::IpDhcpClientEvent event_type) {
                dhcpClientCallback(dhcp_client, event_type);
            });
    } else {
        // Assign static IP configuration.
        iface->iface().setIp4Addr(
            AIpStack::IpIfaceIp4AddrSetting(DevicePrefixLength, DeviceIpAddr));
        iface->iface().setIp4Gateway(
            AIpStack::IpIfaceIp4GatewaySetting(DeviceGatewayAddr));
    }
    
    // Construct the example application.
    auto example_app = std::make_unique<MyExampleApp>(&*stack);
    
    std::fprintf(stderr, "Initialized, entering event loop.\n");
    
    // Run the event loop.
    event_loop.run();
    
    return 0;
}
