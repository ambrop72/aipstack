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

#ifndef AIPSTACK_EXAMPLE_SERVER_H
#define AIPSTACK_EXAMPLE_SERVER_H

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <unordered_set>
#include <stdexcept>
#include <functional>

#include <aipstack/meta/Instance.h>
#include <aipstack/misc/NonCopyable.h>
#include <aipstack/misc/Assert.h>
#include <aipstack/common/Options.h>
#include <aipstack/common/Buf.h>
#include <aipstack/common/Err.h>
#include <aipstack/proto/Ip4Proto.h>
#include <aipstack/proto/IpAddr.h>
#include <aipstack/platform/PlatformFacade.h>

namespace AIpStackExamples {

template <typename Arg>
class ExampleServer :
    private AIpStack::NonCopyable<ExampleServer<Arg>>
{
    using TheIpStack = typename Arg::TheIpStack;
    using Params = typename Arg::Params;
    
    using TcpProto = typename TheIpStack::template GetProtocolType<
        AIpStack::Ip4ProtocolTcp>;
    using TcpListener = typename TcpProto::TcpListener;
    using TcpConnection = typename TcpProto::TcpConnection;
    
    class MyListener : public TcpListener {
    public:
        MyListener (ExampleServer *parent) :
            m_parent(parent)
        {}
        
    private:
        ExampleServer *m_parent;
        
        void connectionEstablished () override final
        {
            m_parent->listenerConnectionEstablished();
        }
    };
    
public:
    ExampleServer (TheIpStack *stack) :
        m_stack(stack),
        m_listener(this)
    {
        if (!m_listener.startListening(tcp(), {
            /*addr=*/ AIpStack::Ip4Addr::ZeroAddr(),
            /*port=*/ Params::EchoPort,
            /*max_pcbs=*/ std::numeric_limits<int>::max()
        })) {
            throw std::runtime_error("ExampleServer: startListening failed.");
        }
        
        m_listener.setInitialReceiveWindow(Params::BufferSize);
    }
    
private:
    inline TcpProto * tcp () const
    {
        return m_stack->template getProtocol<TcpProto>();
    }
    
    void listenerConnectionEstablished ()
    {
        //std::fprintf(stderr, "connectionEstablished\n");
        
        if (m_clients.size() >= Params::MaxClients) {
            std::fprintf(stderr, "Too many clients, rejecting connection.\n");
            return;
        }
        
        try {
            m_clients.emplace(this);
        }
        catch (std::runtime_error const &ex) {
            std::fprintf(stderr, "ERROR: could not create client: %s\n", ex.what());
        }
        catch (std::bad_alloc const &ex) {
            std::fprintf(stderr, "ERROR: could not create client: %s\n", ex.what());
        }
        catch (...) {
            AIPSTACK_ASSERT(false)
        }
    }
    
    class Client :
        private TcpConnection,
        private AIpStack::NonCopyable<Client>
    {
    public:
        Client (ExampleServer *parent) :
            m_parent(parent),
            m_buf_node({m_buffer, Params::BufferSize, &m_buf_node})
        {
            AIpStack::IpErr err = TcpConnection::acceptConnection(&m_parent->m_listener);
            if (err != AIpStack::IpErr::SUCCESS) {
                throw std::runtime_error("TcpConnection::acceptConnection failed");
            }
            
            TcpConnection::setProportionalWindowUpdateThreshold(
                Params::BufferSize, Params::WindowUpdateThresDiv);
            
            TcpConnection::setSendBuf({&m_buf_node, 0, 0});
            TcpConnection::setRecvBuf({&m_buf_node, 0, Params::BufferSize});
        }
        
    private:
        void connectionAborted () override final
        {
            //std::fprintf(stderr, "connectionAborted\n");
            
            m_parent->m_clients.erase(*this);
        }
        
        void dataReceived (std::size_t amount) override final
        {
            //std::fprintf(stderr, "dataReceived %zu\n", amount);
            
            if (amount > 0) {
                TcpConnection::extendSendBuf(amount);
                TcpConnection::sendPush();
            } else {
                TcpConnection::closeSending();
            }
        }
        
        void dataSent (std::size_t amount) override final
        {
            //std::fprintf(stderr, "dataSent %zu\n", amount);
            
            TcpConnection::extendRecvBuf(amount);
        }
        
    private:
        ExampleServer *m_parent;
        AIpStack::IpBufNode m_buf_node;
        char m_buffer[Params::BufferSize];
    };
    
    struct ClientHash {
        std::size_t operator() (Client const &c) const noexcept
        {
            return std::hash<Client const *>{}(&c);
        }
    };
    
    struct ClientEqualTo {
        bool operator() (Client const &c1, Client const &c2) const noexcept
        {
            return &c1 == &c2;
        }
    };
    
private:
    TheIpStack *m_stack;
    MyListener m_listener;
    std::unordered_set<Client, ClientHash, ClientEqualTo> m_clients;
};

struct ExampleServerOptions {
    AIPSTACK_OPTION_DECL_VALUE(EchoPort, std::uint16_t, 2001)
    AIPSTACK_OPTION_DECL_VALUE(BufferSize, std::size_t, 10000)
    AIPSTACK_OPTION_DECL_VALUE(MaxClients, int, 32)
    AIPSTACK_OPTION_DECL_VALUE(WindowUpdateThresDiv, int, 8)
};

template <typename... Options>
class ExampleServerService {
    template <typename>
    friend class ExampleServer;
    
    AIPSTACK_OPTION_CONFIG_VALUE(ExampleServerOptions, EchoPort)
    AIPSTACK_OPTION_CONFIG_VALUE(ExampleServerOptions, BufferSize)
    AIPSTACK_OPTION_CONFIG_VALUE(ExampleServerOptions, MaxClients)
    AIPSTACK_OPTION_CONFIG_VALUE(ExampleServerOptions, WindowUpdateThresDiv)
    
public:
    template <typename TheIpStack_>
    struct Compose {
        using TheIpStack = TheIpStack_;
        using Params = ExampleServerService;
        AIPSTACK_DEF_INSTANCE(Compose, ExampleServer)        
    };
};

}

#endif
