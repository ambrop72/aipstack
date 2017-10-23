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

#ifndef AIPSTACK_TCP_LISTENER_H
#define AIPSTACK_TCP_LISTENER_H

#include <stddef.h>

#include <aipstack/misc/Assert.h>
#include <aipstack/misc/NonCopyable.h>
#include <aipstack/misc/MinMax.h>
#include <aipstack/misc/Preprocessor.h>
#include <aipstack/structure/LinkedList.h>
#include <aipstack/proto/IpAddr.h>
#include <aipstack/tcp/TcpUtils.h>

#include "TcpListener.h"

namespace AIpStack {

#ifndef DOXYGEN_SHOULD_SKIP_THIS
    template <typename> class IpTcpProto;
    template <typename> class IpTcpProto_input;
    template <typename> class TcpConnection;
#endif
    
    /**
     * Structure for listening parameters.
     */
    struct TcpListenParams {
        Ip4Addr addr;
        TcpUtils::PortType port;
        int max_pcbs;
    };
    
    /**
     * Represents listening for connections on a specific address and port.
     */
    template <typename TcpProto>
    class TcpListener :
        private NonCopyable<TcpListener<TcpProto>>
    {
        template <typename> friend class IpTcpProto;
        template <typename> friend class IpTcpProto_input;
        template <typename> friend class TcpConnection;
        
        AIPSTACK_USE_TYPES2(TcpUtils, (PortType, SeqType))
        AIPSTACK_USE_TYPES1(TcpProto, (TcpPcb, Constants))
        
    public:
        /**
         * Initialize the listener.
         * 
         * Upon init, the listener is in not-listening state, and listenIp4 should
         * be called to start listening.
         */
        TcpListener () :
            m_initial_rcv_wnd(0),
            m_accept_pcb(nullptr),
            m_listening(false)
        {}
        
        /**
         * Deinitialize the listener.
         * 
         * All SYN_RCVD connections associated with this listener will be aborted
         * but any already established connection (those associated with a
         * TcpConnection object) will not be affected.
         */
        ~TcpListener ()
        {
            reset();
        }
        
        /**
         * Reset the listener, bringing it to a non-listening state.
         * 
         * This is similar to deinit except that the listener remains initialzied
         * in a default non-listening state.
         */
        void reset ()
        {
            // Stop listening.
            if (m_listening) {
                m_tcp->m_listeners_list.remove(*this);
                m_tcp->unlink_listener(this);
            }
            
            // Reset variables.
            m_initial_rcv_wnd = 0;
            m_accept_pcb = nullptr;
            m_listening = false;
        }
        
        /**
         * Return whether we are listening.
         */
        bool isListening ()
        {
            return m_listening;
        }
        
        /**
         * Return whether a connection is ready to be accepted.
         */
        bool hasAcceptPending ()
        {
            return m_accept_pcb != nullptr;
        }
        
        /**
         * Return a reference to the TCP protocol.
         * 
         * May only be called when listening.
         */
        TcpProto & getTcp ()
        {
            AIPSTACK_ASSERT(isListening())
            
            return *m_tcp;
        }
        
        /**
         * Listen on an IPv4 address and port.
         * 
         * Listening on the all-zeros address listens on all local addresses.
         * Must not be called when already listening.
         * Return success/failure to start listening. It can fail only if there
         * is another listener listening on the same pair of address and port.
         */
        bool startListening (TcpProto *tcp, TcpListenParams const &params)
        {
            AIPSTACK_ASSERT(!m_listening)
            AIPSTACK_ASSERT(tcp != nullptr)
            AIPSTACK_ASSERT(params.max_pcbs > 0)
            
            // Check if there is an existing listener listning on this address+port.
            if (tcp->find_listener(params.addr, params.port) != nullptr) {
                return false;
            }
            
            // Start listening.
            m_tcp = tcp;
            m_addr = params.addr;
            m_port = params.port;
            m_max_pcbs = params.max_pcbs;
            m_num_pcbs = 0;
            m_listening = true;
            m_tcp->m_listeners_list.prepend(*this);
            
            return true;
        }
        
        /**
         * Set the initial receive window used for connections to this listener.
         * 
         * The default initial receive window is 0, which means that a newly accepted
         * connection will not receive data before the user extends the window using
         * extendReceiveWindow.
         * 
         * Note that the initial receive window is applied to a new connection when
         * the SYN is received, not when the connectionEstablished callback is called.
         * Hence the user should generaly use getAnnouncedRcvWnd to determine the
         * initially announced receive window of a new connection. Further, the TCP
         * may still use a smaller initial receive window than configued with this
         * function.
         */
        void setInitialReceiveWindow (size_t rcv_wnd)
        {
            m_initial_rcv_wnd = MinValueU(Constants::MaxWindow, rcv_wnd);
        }
        
    protected:
        /**
         * Called when a new connection has been established.
         * 
         * To accept the connection, the user should call TcpConnection::acceptConnection.
         * Note that there are no special restrictions regarding accessing the connection
         * from within this callback. It is also permissible to deinit/reset the listener.
         * 
         * If the connection is not accepted by acceptConnection within this callback,
         * it will be aborted.
         */
        virtual void connectionEstablished () = 0;
        
    private:
        LinkedListNode<typename TcpProto::ListenerLinkModel> m_listeners_node;
        TcpProto *m_tcp;
        SeqType m_initial_rcv_wnd;
        TcpPcb *m_accept_pcb;
        Ip4Addr m_addr;
        PortType m_port;
        int m_max_pcbs;
        int m_num_pcbs;
        bool m_listening;
    };

}

#endif
