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
#include <cstring>
#include <limits>
#include <memory>
#include <unordered_map>
#include <stdexcept>
#include <utility>

#include <aipstack/misc/NonCopyable.h>
#include <aipstack/misc/Assert.h>
#include <aipstack/misc/Modulo.h>
#include <aipstack/misc/MinMax.h>
#include <aipstack/infra/Options.h>
#include <aipstack/infra/Buf.h>
#include <aipstack/infra/Err.h>
#include <aipstack/infra/Instance.h>
#include <aipstack/proto/Ip4Proto.h>
#include <aipstack/proto/IpAddr.h>
#include <aipstack/platform/PlatformFacade.h>
#include <aipstack/utils/TcpRingBufferUtils.h>

namespace AIpStackExamples {

template <typename Arg>
class ExampleServer :
    private AIpStack::NonCopyable<ExampleServer<Arg>>
{
    using TheIpStack = typename Arg::TheIpStack;
    using Params = typename Arg::Params;
    
    using TcpProto = typename TheIpStack::template GetProtocolType<
        AIpStack::Ip4ProtocolTcp>;
    
    using TcpListener = typename TcpProto::Listener;
    using TcpConnection = typename TcpProto::Connection;
    
    class MyListener : public TcpListener {
    public:
        MyListener (ExampleServer *parent) :
            m_parent(parent)
        {}
        
    private:
        ExampleServer *m_parent;
        
        void connectionEstablished () override final
        {
            m_parent->listenerConnectionEstablished(*this);
        }
    };
    
public:
    ExampleServer (TheIpStack *stack) :
        m_stack(stack),
        m_listener_echo(this),
        m_listener_command(this)
    {
        startListening(m_listener_echo, Params::EchoPort, Params::EchoBufferSize);
        startListening(m_listener_command, Params::LineParsingPort,
                       Params::LineParsingRxBufferSize);
    }
    
private:
    inline TcpProto * tcp () const
    {
        return m_stack->template getProtocol<TcpProto>();
    }
    
    void startListening (MyListener &listener, std::uint16_t port, std::size_t buffer_size)
    {
        if (!listener.startListening(tcp(), {
            /*addr=*/ AIpStack::Ip4Addr::ZeroAddr(),
            /*port=*/ port,
            /*max_pcbs=*/ std::numeric_limits<int>::max()
        })) {
            throw std::runtime_error("ExampleServer: startListening failed.");
        }
        
        listener.setInitialReceiveWindow(buffer_size);
    }
    
    void listenerConnectionEstablished (MyListener &listener)
    {
        if (m_clients.size() >= Params::MaxClients) {
            std::fprintf(stderr, "Too many clients, rejecting connection.\n");
            return;
        }
        
        try {
            // Construct the appropriate BaseClient-derived object.
            std::unique_ptr<BaseClient> client;
            if (&listener == &m_listener_echo) {
                client.reset(new EchoClient(this, listener));
            } else {
                client.reset(new LineParsingClient(this, listener));
            }
            
            // Insert the object into m_clients.
            BaseClient *clientPtr = &*client;
            m_clients.insert(std::make_pair(clientPtr, std::move(client)));
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
    
    class BaseClient :
        protected TcpConnection,
        private AIpStack::NonCopyable<BaseClient>
    {
    public:
        BaseClient (ExampleServer *parent, MyListener &listener) :
            m_parent(parent)
        {
            // Setup the TcpConnection by accepting a connection on the listener.
            AIpStack::IpErr err = TcpConnection::acceptConnection(&listener);
            if (err != AIpStack::IpErr::SUCCESS) {
                throw std::runtime_error("TcpConnection::acceptConnection failed");
            }
            
            std::fprintf(stderr, "Connection established.\n");
        }
        
        virtual ~BaseClient () {}
        
    protected:
        // This is used to destroy the client object. It does this by removing
        // it from m_clients. Note that after this, the client object no longer
        // exists and therefore must not be accessed in any way.
        void destroy (bool have_unprocessed_data)
        {
            TcpConnection::reset(have_unprocessed_data);
            
            std::size_t removed = m_parent->m_clients.erase(this);
            AIPSTACK_ASSERT(removed == 1)
            (void)removed;
        }
        
    private:
        // This is called by TcpConnection when the connection has transitioned to
        // the CLOSED state. Since communication with the client is no longer
        // possible, this immediately destroys the client object.
        void connectionAborted () override final
        {
            std::fprintf(stderr, "Connection aborted.\n");
            
            return destroy(false);
        }
        
    protected:
        ExampleServer *m_parent;
    };
    
    // The echo client uses the same circular buffer for sending and receiving,
    // there is no copying between buffers. Due to this the implementation is
    // very simple.
    class EchoClient : public BaseClient
    {
    public:
        EchoClient (ExampleServer *parent, MyListener &listener) :
            BaseClient(parent, listener),
            m_buf_node({m_buffer, Params::EchoBufferSize, &m_buf_node})
        {
            TcpConnection::setProportionalWindowUpdateThreshold(
                Params::EchoBufferSize, Params::WindowUpdateThresDiv);
            
            TcpConnection::setRecvBuf({&m_buf_node, 0, Params::EchoBufferSize});
            TcpConnection::setSendBuf({&m_buf_node, 0, 0});
        }
        
    private:
        void dataReceived (std::size_t amount) override final
        {
            if (amount > 0) {
                TcpConnection::extendSendBuf(amount);
                TcpConnection::sendPush();
            } else {
                TcpConnection::closeSending();
            }
        }
        
        void dataSent (std::size_t amount) override final
        {
            TcpConnection::extendRecvBuf(amount);
        }
        
    private:
        AIpStack::IpBufNode m_buf_node;
        char m_buffer[Params::EchoBufferSize];
    };
    
    // The line parsing client parses the received data into lines and upon each complete
    // line sends back "Line: " followed by the received line (including the newline).
    // It uses separate receive and send buffers.
    class LineParsingClient : public BaseClient
    {
        static constexpr char const ResponsePrefix[] = "Line: ";
        static std::size_t const ResponsePrefixLen = sizeof(ResponsePrefix) - 1;
        
        static std::size_t const RxBufSize = Params::LineParsingRxBufferSize;
        static std::size_t const TxBufSize = Params::LineParsingTxBufferSize;
        static std::size_t const MaxRxLineLen = Params::LineParsingMaxRxLineLen;
        
        static_assert(MaxRxLineLen <= RxBufSize, "");
        static_assert(TxBufSize >= ResponsePrefixLen + MaxRxLineLen, "");
        
        enum class State {RecvLine, WaitRespBuf, WaitFinSent};
        
    public:
        LineParsingClient (ExampleServer *parent, MyListener &listener) :
            BaseClient(parent, listener),
            m_rx_line_len(0),
            m_state(State::RecvLine)
        {
            m_rx_ring_buf.setup(*this, m_rx_buffer, RxBufSize,
                                Params::WindowUpdateThresDiv);
            m_tx_ring_buf.setup(*this, m_tx_buffer, TxBufSize);
        }
        
    private:
        void dataReceived (std::size_t amount) override final
        {
            if (m_state == State::RecvLine) {
                return processReceived();
            }
        }
        
        void dataSent (std::size_t amount) override final
        {
            if (m_state == State::WaitRespBuf) {
                // Re-try transferring the line to the send buffer.
                if (writeResponse()) {
                    // Line has been transferred, continue processing received data.
                    m_state = State::RecvLine;
                    return processReceived();
                }
            }
            
            // NOTE: In WaitFinSent state, we could check TcpConnection::wasEndSent()
            // and if that is true (FIN has been acknowledged) call destroy(), but it
            // would be redundant since connectionAborted() would anyway be called just
            // after that.
        }
        
        void processReceived ()
        {
            AIPSTACK_ASSERT(m_state == State::RecvLine)
            
            while (true) {
                AIPSTACK_ASSERT(m_rx_line_len <= MaxRxLineLen)
                
                // Get the range of received data.
                AIpStack::IpBufRef rx_data = m_rx_ring_buf.getReadRange(*this);
                
                // Skip over any already parsed data (known not to contain a newline).
                AIpStack::IpBufRef unparsed_data = rx_data;
                unparsed_data.skipBytes(m_rx_line_len);

                // Search for a newline.
                bool found_newline = unparsed_data.findByte(
                    '\n', MaxRxLineLen - m_rx_line_len);

                // Update m_rx_line_len to reflect any data searched possibly including
                // a newline that was just found.
                m_rx_line_len = rx_data.tot_len - unparsed_data.tot_len;
                
                // No newline yet?
                if (!found_newline) {
                    // Check if the line is too long, if so then disconnect the client.
                    if (m_rx_line_len >= MaxRxLineLen) {
                        std::fprintf(stderr, "Line too long, disconnecting client.\n");
                        return BaseClient::destroy(true);
                    }

                    if (TcpConnection::wasEndReceived()) {
                        // FIN has been encountered. Call closeSending to send our own
                        // FIN and go to state WaitFinSent where we wait until our FIN
                        // is acknowledged. This will be reported by dataSent(0) but
                        // we do not use that (see the comment in dataSent).
                        TcpConnection::closeSending();
                        m_state = State::WaitFinSent;
                    } else {
                        // We will wait for more data.
                    }
                    return;
                }
                
                // Try to transfer the received line the send buffer.
                if (!writeResponse()) {
                    // Not enough space in the send buffer, go to WaitRespBuf state
                    // in order to retry from dataSent. We will not process received
                    // data until the line is transferred.
                    m_state = State::WaitRespBuf;
                    return;
                }
                
                // writeResponse has transferred the line, updated TCP buffers and reset
                // m_rx_line_len; continue parsing the next line.
            }
        }
        
        bool writeResponse ()
        {
            // We will transfer the prefix and the received line to the send buffer.
            // Get the length of the line and calculate the length of the response.
            std::size_t recv_len = m_rx_line_len;
            std::size_t response_len = ResponsePrefixLen + recv_len;
            
            // Get the range of free space in the send buffer.
            AIpStack::IpBufRef tx_free = m_tx_ring_buf.getWriteRange(*this);

            // Check if there is sufficient space for the response in the send buffer.
            if (tx_free.tot_len < response_len) {
                return false;
            }
            
            // Get the range of bytes where the received line is.
            AIpStack::IpBufRef rx_line =
                m_rx_ring_buf.getReadRange(*this).subTo(recv_len);
            
            // Write the response prefix to the send buffer.
            tx_free.giveBytes(ResponsePrefixLen, ResponsePrefix);
            
            // Copy the line from the receive buffer to the send buffer.
            tx_free.giveBuf(rx_line);
            
            // Extend the receive buffer (mark the space used by the received line as free).
            TcpConnection::extendRecvBuf(recv_len);
            
            // Extend the send buffer (mark the space just written as used).
            TcpConnection::extendSendBuf(response_len);
            
            // Push to make sure that sending is not delayed indefinitely.
            TcpConnection::sendPush();
            
            // Reset m_rx_line_len so that processing of the next line can start.
            m_rx_line_len = 0;
            
            return true;
        }
        
    private:
        AIpStack::RecvRingBuffer<TcpProto> m_rx_ring_buf;
        AIpStack::SendRingBuffer<TcpProto> m_tx_ring_buf;
        std::size_t m_rx_line_len;
        State m_state;
        char m_rx_buffer[RxBufSize];
        char m_tx_buffer[TxBufSize];
    };
    
private:
    TheIpStack *m_stack;
    MyListener m_listener_echo;
    MyListener m_listener_command;
    std::unordered_map<BaseClient *, std::unique_ptr<BaseClient>> m_clients;
};

template <typename Arg>
constexpr char const ExampleServer<Arg>::LineParsingClient::ResponsePrefix[];

struct ExampleServerOptions {
    AIPSTACK_OPTION_DECL_VALUE(EchoPort, std::uint16_t, 2001)
    AIPSTACK_OPTION_DECL_VALUE(EchoBufferSize, std::size_t, 10000)
    AIPSTACK_OPTION_DECL_VALUE(LineParsingPort, std::uint16_t, 2002)
    AIPSTACK_OPTION_DECL_VALUE(LineParsingRxBufferSize, std::size_t, 6000)
    AIPSTACK_OPTION_DECL_VALUE(LineParsingTxBufferSize, std::size_t, 6000)
    AIPSTACK_OPTION_DECL_VALUE(LineParsingMaxRxLineLen, std::size_t, 200)
    AIPSTACK_OPTION_DECL_VALUE(MaxClients, int, 32)
    AIPSTACK_OPTION_DECL_VALUE(WindowUpdateThresDiv, int, 8)
};

template <typename... Options>
class ExampleServerService {
    template <typename>
    friend class ExampleServer;
    
    AIPSTACK_OPTION_CONFIG_VALUE(ExampleServerOptions, EchoPort)
    AIPSTACK_OPTION_CONFIG_VALUE(ExampleServerOptions, EchoBufferSize)
    AIPSTACK_OPTION_CONFIG_VALUE(ExampleServerOptions, LineParsingPort)
    AIPSTACK_OPTION_CONFIG_VALUE(ExampleServerOptions, LineParsingRxBufferSize)
    AIPSTACK_OPTION_CONFIG_VALUE(ExampleServerOptions, LineParsingTxBufferSize)
    AIPSTACK_OPTION_CONFIG_VALUE(ExampleServerOptions, LineParsingMaxRxLineLen)
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
