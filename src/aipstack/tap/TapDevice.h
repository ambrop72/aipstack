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

#ifndef AIPSTACK_TAP_DEVICE_H
#define AIPSTACK_TAP_DEVICE_H

#if defined(__linux__)
#include <aipstack/tap/linux/TapDeviceLinux.h>
#elif defined(_WIN32)
#include <aipstack/tap/windows/TapDeviceWindows.h>
#else
#error "TapDevice is not supported on this platform"
#endif

namespace AIpStack {

/**
 * @defgroup tap TAP Virtual Ethernet Device
 * @brief Provides access to a TAP virtual Ethernet device driver.
 * 
 * See the @ref TapDevice documentation.
 * 
 * @{
 */

#ifdef IN_DOXYGEN

/**
 * Provides access to a TAP virtual Ethernet device driver.
 * 
 * This facility relies on the @ref event-loop implementation in %AIpStack.
 * 
 * The following platforms are currently supported:
 * - Linux: Uses the TUN/TAP driver that comes with the kernel.
 * - Windows: Uses the TAP-Windows driver.
 * 
 * After a @ref TapDevice object is constructed, frames received from the driver
 * will be reported via the @ref FrameReceivedHandler callback function and
 * frames can be sent to the driver using @ref sendFrame.
 * 
 * Note that there is no actual "TapDevice" class, but only a type alias for a
 * class which provides the interface described here for a specific platform
 * (e.g. TapDeviceLinux or TapDeviceWindows).
 */
class TapDevice :
    private AIpStack::NonCopyable<TapDevice>
{
public:
    /**
     * Type of callback used to deliver frames received from the driver, which
     * were sent as outgoing frames by the OS.
     * 
     * @param frame Frame data (referenced using @ref IpBufRef), starting with the
     *        14-byte Ethernet header. The referenced buffers must not be used
     *        outside of the callback function.
     */
    using FrameReceivedHandler = Function<void(AIpStack::IpBufRef frame)>;

    /**
     * Constructor, initializes the driver and related resources.
     * 
     * The meaning of the `device_id` argument depends on the platform:
     * - Linux: it should be the name of an existing TAP network interface.
     * - Windows: it should be `tap0901:<adapter_name>` where `<adapter_name>` is
     *   the name of a TAP-Windows device as seen in "Network Connections".
     *   Non-ASCII characters in the name are not supported.
     * 
     * @param loop Event loop; it must outlive the TapDevice object.
     * @param device_id Specifies which virtual Ethernet device to use (see
     *        description).
     * @param handler Callback function used to deliver Ethernet frames received
     *        from the driver (must not be null).
     */
    TapDevice (AIpStack::EventLoop &loop, std::string const &device_id,
               FrameReceivedHandler handler);
    
    /**
     * Destructor, disconnects from the driver and releases resources.
     */
    ~TapDevice ();
    
    /**
     * Get the IP MTU of the virtual Ethernet device.
     * 
     * @return The IP MTU, not including the 14-byte Ethernet header.
     */
    std::size_t getMtu () const;

    /**
     * Send an Ethernet frame to the driver, which will be processed by the OS
     * as an incoming frame.
     * 
     * @param frame Frame data (referenced using @ref IpBufRef), starting with
     *        the 14-byte Ethernet header.
     * @return Success or error code.
     */
    AIpStack::IpErr sendFrame (AIpStack::IpBufRef frame);
};

#else

#if defined(__linux__)
using TapDevice = TapDeviceLinux;
#elif defined(_WIN32)
using TapDevice = TapDeviceWindows;
#endif

#endif

/** @} */

}

#endif
