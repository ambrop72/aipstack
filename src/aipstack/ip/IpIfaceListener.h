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

#ifndef AIPSTACK_IP_IFACE_LISTENER_H
#define AIPSTACK_IP_IFACE_LISTENER_H

#include <cstdint>

#include <aipstack/misc/NonCopyable.h>
#include <aipstack/misc/Function.h>
#include <aipstack/structure/LinkedList.h>
#include <aipstack/infra/Buf.h>
#include <aipstack/ip/IpStackTypes.h>

namespace AIpStack {

#ifndef IN_DOXYGEN
template <typename> class IpStack;
template <typename> class IpIface;
#endif

/**
 * @addtogroup ip-stack
 * @{
 */

/**
 * Allows receiving and intercepting IP datagrams received through a specific
 * interface with a specific IP protocol.
 * 
 * @tparam Arg Template parameter of @ref IpStack.
 */
template <typename Arg>
class IpIfaceListener :
    private NonCopyable<IpIfaceListener<Arg>>
{
    template <typename> friend class IpStack;
    
public:
    /**
     * Type of callback called when a matching datagram is received.
     * 
     * This is called before passing the datagram to any protocol handler. The return value
     * allows inhibiting further processing of the datagram (by other @ref IpIfaceListener
     * "IpIfaceListener"s, protocol handlers and built-in protocols such as ICMP).
     * 
     * @warning It is not allowed to deinitialize this listener object from this callback
     * unless true is returned.
     * 
     * @warning It is not allowed to remove the interface through which the packet has been
     * received from this callback.
     * 
     * @param ip_info Information about the received datagram.
     * @param dgram Data of the received datagram.
     * @return True to inhibit further processing, false to continue.
     */
    using Ip4DgramHandler = Function<bool(IpRxInfoIp4<Arg> const &ip_info, IpBufRef dgram)>;

    /**
     * Construct the listener object and start listening.
     * 
     * @param iface The interface to listen for packets on. It is the
     *        responsibility of the user to ensure that the interface is
     *        not removed while this object is still initialized.
     * @param proto IP protocol number that the user is interested on.
     * @param ip4_handler Callback function to which matching received datagrams
     *        will be passed (must not be null).
     */
    IpIfaceListener (IpIface<Arg> *iface, std::uint8_t proto, Ip4DgramHandler ip4_handler) :
        m_iface(iface),
        m_proto(proto),
        m_ip4_handler(ip4_handler)
    {
        m_iface->m_listeners_list.prepend(*this);
    }
    
    /**
     * Destruct the listener object.
     */
    ~IpIfaceListener ()
    {
        m_iface->m_listeners_list.remove(*this);
    }
    
    /**
     * Return the interface on which this object is listening.
     * 
     * @return Interface on which this object is listening.
     */
    inline IpIface<Arg> * getIface () const
    {
        return m_iface;
    }
    
private:
    LinkedListNode<typename IpStack<Arg>::IfaceListenerLinkModel> m_list_node;
    IpIface<Arg> *m_iface;
    std::uint8_t m_proto;
    Ip4DgramHandler m_ip4_handler;
};

/** @} */

}

#endif
