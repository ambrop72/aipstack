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
#include <aipstack/misc/Function.h>
#include <aipstack/misc/MemRef.h>
#include <aipstack/infra/Options.h>
#include <aipstack/infra/Buf.h>
#include <aipstack/infra/Err.h>
#include <aipstack/infra/Instance.h>
#include <aipstack/proto/Ip4Proto.h>
#include <aipstack/platform/PlatformFacade.h>
#include <aipstack/ip/IpAddr.h>
#include <aipstack/ip/IpStack.h>
#include <aipstack/tcp/TcpApi.h>
#include <aipstack/tcp/TcpListener.h>
#include <aipstack/tcp/TcpConnection.h>
#include <aipstack/utils/TcpRingBufferUtils.h>
#include <aipstack/utils/IpAddrFormat.h>
#include <aipstack/utils/IntFormat.h>

namespace AIpStackExamples {

template <typename Arg>
class ExampleApp :
    private AIpStack::NonCopyable<ExampleApp<Arg>>
{
    using StackArg = typename Arg::StackArg;
    using Params = typename Arg::Params;
    
    using IpStack = AIpStack::IpStack<StackArg>;

    using TcpArg = typename IpStack::template GetProtoArg<AIpStack::TcpApi>;
    using TcpListener = AIpStack::TcpListener<TcpArg>;
    using TcpConnection = AIpStack::TcpConnection<TcpArg>;

    // Function type used in the BaseClient constructor to initialize the connection
    // (e.g. accept or start connection).
    using ClientSetupFunc = AIpStack::Function<void(TcpConnection &)>;

    enum class ClientType {Echo, Command};
    
public:
    ExampleApp (IpStack *stack) :
        m_stack(stack),
        m_listener_echo(
            AIPSTACK_BIND_MEMBER_TN(&ExampleApp::connectionEstablishedEcho, this)),
        m_listener_command(
            AIPSTACK_BIND_MEMBER_TN(&ExampleApp::connectionEstablishedCommand, this))
    {
        startListening(m_listener_echo, Params::EchoPort, Params::EchoBufferSize);
        startListening(m_listener_command, Params::LineParsingPort,
                       Params::LineParsingRxBufferSize);
    }
    
private:
    inline AIpStack::TcpApi<TcpArg> & tcp () const
    {
        return m_stack->template getProtoApi<AIpStack::TcpApi>();
    }
    
    void startListening (TcpListener &listener, std::uint16_t port, std::size_t buffer_size)
    {
        if (!listener.startListening(tcp(), {
            /*addr=*/ AIpStack::Ip4Addr::ZeroAddr(),
            /*port=*/ port,
            /*max_pcbs=*/ std::numeric_limits<int>::max()
        })) {
            throw std::runtime_error("ExampleApp: startListening failed.");
        }
        
        listener.setInitialReceiveWindow(buffer_size);
    }

    void connectionEstablishedEcho ()
    {
        return connectionEstablishedCommon(m_listener_echo);
    }

    void connectionEstablishedCommand ()
    {
        return connectionEstablishedCommon(m_listener_command);
    }
    
    void connectionEstablishedCommon (TcpListener &listener)
    {
        ClientType type = (&listener == &m_listener_echo) ?
            ClientType::Echo : ClientType::Command;
        
        createConnection(type, [&listener](TcpConnection &con) {
            // This is called from the BaseClient constructor to setup the connection.
            AIpStack::IpErr err = con.acceptConnection(listener);
            if (err != AIpStack::IpErr::Success) {
                throw std::runtime_error("TcpConnection::acceptConnection failed");
            }
        });
    }

    void createConnection (ClientType type, ClientSetupFunc setupFunc)
    {
        if (m_clients.size() >= Params::MaxClients) {
            std::fprintf(stderr, "Too many clients, rejecting connection.\n");
            return;
        }
        
        try {
            // Construct the appropriate BaseClient-derived object.
            std::unique_ptr<BaseClient> client;
            if (type == ClientType::Echo) {
                client.reset(new EchoClient(this, setupFunc));
            } else {
                client.reset(new LineParsingClient(this, setupFunc));
            }
            
            // Insert the object into m_clients.
            BaseClient *clientPtr = &*client;
            m_clients.insert(std::make_pair(clientPtr, std::move(client)));
        }
        catch (std::exception const &ex) {
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
        BaseClient (ExampleApp *parent, ClientSetupFunc setupFunc) :
            m_parent(parent)
        {
            setupFunc(*this);

            // Remember addresses needed by log() since these cannot be retrieved after
            // connectionAborted().
            m_local_addr = TcpConnection::getLocalIp4Addr();
            m_remote_addr = TcpConnection::getRemoteIp4Addr();
            m_local_port = TcpConnection::getLocalPort();
            m_remote_port = TcpConnection::getRemotePort();

            log("Connection established.");
        }
        
        virtual ~BaseClient () {}

    protected:
        void log (char const *msg) const
        {
            char buf[200];
            char *pos = buf;
            char *end = buf + sizeof(buf) - 1;

            *pos++ = '(';
            pos = AIpStack::FormatIpAddr(pos, m_local_addr);
            *pos++ = ':';
            pos = AIpStack::FormatInteger(pos, m_local_port);
            *pos++ = ' ';
            pos = AIpStack::FormatIpAddr(pos, m_remote_addr);
            *pos++ = ':';
            pos = AIpStack::FormatInteger(pos, m_remote_port);
            *pos++ = ')';
            *pos++ = ' ';
            pos += std::snprintf(pos, std::size_t(end - pos), "%s", msg);
            *pos = '\0';

            std::fprintf(stderr, "%s\n", buf);
        }

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

        inline ExampleApp & parent () const { return *m_parent; }
        
    private:
        // This is called by TcpConnection when the connection has transitioned to
        // the CLOSED state. Since communication with the client is no longer
        // possible, this immediately destroys the client object.
        void connectionAborted () override final
        {
            log("Connection aborted.");
            
            return destroy(false);
        }
        
    private:
        ExampleApp *m_parent;
        AIpStack::Ip4Addr m_local_addr;
        AIpStack::Ip4Addr m_remote_addr;
        std::uint16_t m_local_port;
        std::uint16_t m_remote_port;
    };
    
    // The echo client uses the same circular buffer for sending and receiving,
    // there is no copying between buffers. Due to this the implementation is
    // very simple.
    class EchoClient : public BaseClient
    {
    public:
        EchoClient (ExampleApp *parent, ClientSetupFunc setupFunc) :
            BaseClient(parent, setupFunc),
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
        LineParsingClient (ExampleApp *parent, ClientSetupFunc setupFunc) :
            BaseClient(parent, setupFunc),
            m_rx_line_len(0),
            m_state(State::RecvLine)
        {
            m_rx_ring_buf.setup(*this, m_rx_buffer, RxBufSize,
                                Params::WindowUpdateThresDiv);
            m_tx_ring_buf.setup(*this, m_tx_buffer, TxBufSize);
        }

        using BaseClient::log;
        
    private:
        void dataReceived (std::size_t amount) override final
        {
            (void)amount;

            if (m_state == State::RecvLine) {
                return processReceived();
            }
        }
        
        void dataSent (std::size_t amount) override final
        {
            (void)amount;
            
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
                        log("Line too long, disconnecting client.");
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

                // Process the line for commands.
                processLine(rx_data.subTo(m_rx_line_len - 1));
                
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
            tx_free.giveBytes(AIpStack::MemRef(ResponsePrefix, ResponsePrefixLen));
            
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

        void processLine (AIpStack::IpBufRef line_ref)
        {
            using namespace AIpStack::MemRefLiterals;
            constexpr std::size_t BufSize = 100;

            // Check if the line fits into BufSize.
            if (line_ref.tot_len > BufSize) {
                return;
            }

            // Copy the line into a local buffer.
            char buf[BufSize];
            AIpStack::MemRef line(buf, line_ref.tot_len);
            line_ref.takeBytes(line.len, buf);
            
            if (line.removePrefix("connect ")) {
                std::size_t colon_pos;
                if (!line.findChar(':', colon_pos)) {
                    return;
                }

                AIpStack::Ip4Addr addr;
                if (!AIpStack::ParseIpAddr(line.subTo(colon_pos), addr)) {
                    return;
                }

                std::uint16_t port;
                if (!AIpStack::ParseInteger(line.subFrom(colon_pos + 1), port)) {
                    return;
                }

                AIpStack::TcpStartConnectionArgs<TcpArg> con_args;
                con_args.addr = addr;
                con_args.port = port;
                con_args.rcv_wnd = Params::EchoBufferSize;

                ExampleApp &parent = BaseClient::parent();

                parent.createConnection(ClientType::Echo, AIpStack::RefFunc(
                [&](TcpConnection &con) {
                    // This is called from the BaseClient constructor to setup the
                    // connection.
                    AIpStack::IpErr err = con.startConnection(parent.tcp(), con_args);
                    if (err != AIpStack::IpErr::Success) {
                        throw std::runtime_error("TcpConnection::startConnection failed");
                    }
                }));
            }
        }
        
    private:
        AIpStack::RecvRingBuffer<TcpArg> m_rx_ring_buf;
        AIpStack::SendRingBuffer<TcpArg> m_tx_ring_buf;
        std::size_t m_rx_line_len;
        State m_state;
        char m_rx_buffer[RxBufSize];
        char m_tx_buffer[TxBufSize];
    };
    
private:
    IpStack *m_stack;
    TcpListener m_listener_echo;
    TcpListener m_listener_command;
    std::unordered_map<BaseClient *, std::unique_ptr<BaseClient>> m_clients;
};

template <typename Arg>
constexpr char const ExampleApp<Arg>::LineParsingClient::ResponsePrefix[];

struct ExampleAppOptions {
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
class ExampleAppService {
    template <typename>
    friend class ExampleApp;
    
    AIPSTACK_OPTION_CONFIG_VALUE(ExampleAppOptions, EchoPort)
    AIPSTACK_OPTION_CONFIG_VALUE(ExampleAppOptions, EchoBufferSize)
    AIPSTACK_OPTION_CONFIG_VALUE(ExampleAppOptions, LineParsingPort)
    AIPSTACK_OPTION_CONFIG_VALUE(ExampleAppOptions, LineParsingRxBufferSize)
    AIPSTACK_OPTION_CONFIG_VALUE(ExampleAppOptions, LineParsingTxBufferSize)
    AIPSTACK_OPTION_CONFIG_VALUE(ExampleAppOptions, LineParsingMaxRxLineLen)
    AIPSTACK_OPTION_CONFIG_VALUE(ExampleAppOptions, MaxClients)
    AIPSTACK_OPTION_CONFIG_VALUE(ExampleAppOptions, WindowUpdateThresDiv)
    
public:
    template <typename StackArg_>
    struct Compose {
        using StackArg = StackArg_;
        using Params = ExampleAppService;
        AIPSTACK_DEF_INSTANCE(Compose, ExampleApp) 
    };
};

}

#endif
