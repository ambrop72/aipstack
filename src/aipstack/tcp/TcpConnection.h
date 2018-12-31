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

#ifndef AIPSTACK_TCP_CONNECTION_H
#define AIPSTACK_TCP_CONNECTION_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include <aipstack/misc/Assert.h>
#include <aipstack/misc/MinMax.h>
#include <aipstack/misc/NonCopyable.h>
#include <aipstack/misc/Hints.h>
#include <aipstack/infra/Buf.h>
#include <aipstack/infra/Err.h>
#include <aipstack/ip/IpAddr.h>
#include <aipstack/ip/IpMtuRef.h>
#include <aipstack/tcp/TcpState.h>
#include <aipstack/tcp/TcpSeqNum.h>
#include <aipstack/tcp/TcpListener.h>

namespace AIpStack {

#ifndef IN_DOXYGEN
template <typename> class IpTcpProto;
template <typename> class IpTcpProto_input;
template <typename> class IpTcpProto_output;
template <typename> class TcpApi;
#endif

/**
 * Encapsulates connection parameters for @ref TcpConnection::startConnection.
 */
template <typename Arg>
struct TcpStartConnectionArgs {
    Ip4Addr addr = Ip4Addr::ZeroAddr();
    std::uint16_t port = 0;
    std::size_t rcv_wnd = 0;
};

/**
 * Represents a TCP connection.
 * Conceptually, the connection object has three main states:
 * - INIT: No connection has been made yet.
 * - CONNECTED: There is an active connection, or a connection attempt
 *              is in progress.
 * - CLOSED: There was a connection but is no more.
 */
template <typename Arg>
class TcpConnection :
    private NonCopyable<TcpConnection<Arg>>
#ifndef IN_DOXYGEN
    ,private IpMtuRef<typename Arg::StackArg>
#endif
{
    template <typename> friend class IpTcpProto;
    template <typename> friend class IpTcpProto_input;
    template <typename> friend class IpTcpProto_output;
    
    using TcpConStackArg = typename Arg::StackArg;
    
    using TcpConProto = IpTcpProto<Arg>;
    using TcpConPcb = typename TcpConProto::TcpPcb;
    using TcpConInput = typename TcpConProto::Input;
    using TcpConOutput = typename TcpConProto::Output;
    using TcpConConstants = typename TcpConProto::Constants;
    using TcpConOosBuffer = typename TcpConProto::OosBuffer;

public:
    /**
     * Initializes the connection object.
     * The object is initialized in INIT state.
     */
    TcpConnection ()
    {
        m_v.pcb = nullptr;
        reset_flags();
    }
    
    /**
     * Resets the connection object.
     * This brings the object to INIT state.
     * In CONNECTED state, the `have_unprocessed_data` argument should indicate whether
     * the application has any buffered unprocessed received data, so that the TCP can
     * send RST (see RFC 2525 section 2.17). In other states (including after
     * connectionAborted) this argument is irrelevant.
     */
    void reset (bool have_unprocessed_data = false)
    {
        if (m_v.pcb != nullptr) {
            assert_started();
            
            TcpConPcb *pcb = m_v.pcb;
            
            // Reset the MtuRef.
            mtu_ref().reset(pcb->tcp->m_stack);
            
            // Disassociate with the PCB.
            pcb->con = nullptr;
            m_v.pcb = nullptr;
            
            // Handle abandonment of connection.
            bool rst_needed = m_v.snd_buf.tot_len > 0 || have_unprocessed_data;
            TcpConProto::pcb_abandoned(pcb, rst_needed, m_v.rcv_ann_thres);
        }
        
        reset_flags();
    }
    
    /**
     * Accepts a connection available on a listener.
     * On success, this brings the object from the INIT to CONNECTED state.
     * On failure, the object remains in INIT state.
     * May only be called in INIT state.
     */
    IpErr acceptConnection (TcpListener<Arg> &lis)
    {
        assert_init();
        AIPSTACK_ASSERT(lis.m_accept_pcb != nullptr)
        AIPSTACK_ASSERT(lis.m_accept_pcb->state() == TcpStates::SYN_RCVD)
        AIPSTACK_ASSERT(lis.m_accept_pcb->lis == &lis)
        
        TcpConPcb *pcb = lis.m_accept_pcb;
        TcpConProto *tcp = pcb->tcp;
        
        // Setup the MTU reference.
        std::uint16_t pmtu;
        if (!mtu_ref().setup(tcp->m_stack, pcb->remote_addr, nullptr, pmtu)) {
            return IpErr::NoMtuEntryAvailable;
        }
        
        // Clear the m_accept_pcb link from the listener.
        lis.m_accept_pcb = nullptr;
        
        // Decrement the listener's PCB count.
        AIPSTACK_ASSERT(lis.m_num_pcbs > 0)
        lis.m_num_pcbs--;
        
        // Note that the PCB has already been removed from the list of
        // unreferenced PCBs, so we must not try to remove it here.
        
        // Set the PCB state to ESTABLISHED.
        pcb->setState(TcpStates::ESTABLISHED);
        
        // Associate with the PCB.
        m_v.pcb = pcb;
        pcb->con = this;
        
        // Initialize TcpConnection variables, set STARTED flag.
        setup_common_started();
        
        // Initialize certain sender variables.
        TcpConInput::pcb_complete_established_transition(pcb, pmtu);
        
        return IpErr::Success;
    }
    
    /**
     * Starts a connection attempt to the given address.
     * When connection is fully established, the connectionEstablished
     * callback will be called. But otherwise a connection that is
     * conneecting does not behave differently from a fully connected
     * connection and for that reason the connectionEstablished callback
     * need not be implemented.
     * On success, this brings the object from the INIT to CONNECTED state.
     * On failure, the object remains in INIT state.
     * May only be called in INIT state.
     */
    IpErr startConnection (TcpApi<Arg> &api, TcpStartConnectionArgs<Arg> const &args)
    {
        assert_init();

        TcpConProto &tcp = api.proto();
        
        // Setup the MTU reference.
        std::uint16_t pmtu;
        if (!mtu_ref().setup(tcp.m_stack, args.addr, nullptr, pmtu)) {
            return IpErr::NoMtuEntryAvailable;
        }
        
        // Create the PCB for the connection.
        TcpConPcb *pcb = nullptr;
        IpErr err = tcp.create_connection(this, args, pmtu, &pcb);
        if (err != IpErr::Success) {
            mtu_ref().reset(tcp.m_stack);
            return err;
        }
        
        // Remember the PCB (the link to us already exists).
        AIPSTACK_ASSERT(pcb != nullptr)
        AIPSTACK_ASSERT(pcb->con == this)
        m_v.pcb = pcb;
        
        // Initialize TcpConnection variables, set STARTED flag.
        setup_common_started();
        
        return IpErr::Success;
    }
    
    /**
     * Move a connection from another connection object.
     * The source connection object must be in CONNECTED state.
     * This brings this object from the INIT to CONNECTED state.
     * May only be called in INIT state.
     */
    void moveConnection (TcpConnection *src_con)
    {
        assert_init();
        src_con->assert_connected();
        
        static_assert(std::is_trivially_copy_constructible<TcpConOosBuffer>::value);
        
        // Byte-copy the whole m_v.
        std::memcpy(&m_v, &src_con->m_v, sizeof(m_v));
        
        // Update the PCB association.
        m_v.pcb->con = this;
        
        // Move the MtuRef setup.
        mtu_ref().moveFrom(*src_con);
        
        // Reset the source.
        src_con->m_v.pcb = nullptr;
        src_con->reset_flags();
    }
    
    /**
     * Returns whether the object is in INIT state.
     */
    inline bool isInit () const
    {
        return !m_v.started;
    }
    
    /**
     * Returns whether the object is in CONNECTED state.
     */
    inline bool isConnected () const
    {
        return m_v.pcb != nullptr;
    }
    
    /**
     * Return a reference to the TCP protocol API.
     * 
     * May only be called in CONNECTED state.
     */
    TcpApi<Arg> & getApi () const
    {
        AIPSTACK_ASSERT(isConnected())
        
        return *m_v.pcb->tcp;
    }
    
    /**
     * Return the local port number.
     * 
     * May only be called in CONNECTED state.
     */
    std::uint16_t getLocalPort () const
    {
        AIPSTACK_ASSERT(isConnected())
        
        return m_v.pcb->local_port;
    }
    
    /**
     * Return the remote port number.
     * 
     * May only be called in CONNECTED state.
     */
    std::uint16_t getRemotePort () const
    {
        AIPSTACK_ASSERT(isConnected())
        
        return m_v.pcb->remote_port;
    }
    
    /**
     * Return the local IPv4 address.
     * 
     * May only be called in CONNECTED state.
     */
    Ip4Addr getLocalIp4Addr () const
    {
        AIPSTACK_ASSERT(isConnected())
        
        return m_v.pcb->local_addr;
    }
    
    /**
     * Return the remote IPv4 address.
     * 
     * May only be called in CONNECTED state.
     */
    Ip4Addr getRemoteIp4Addr () const
    {
        AIPSTACK_ASSERT(isConnected())
        
        return m_v.pcb->remote_addr;
    }
    
    /**
     * Sets the window update threshold.
     * If the threshold is being raised outside of initializing a new
     * connection, is advised to then call extendRecvBuf(0) which will
     * ensure that a window update is sent if it is now needed.
     * May only be called in CONNECTED or CLOSED state.
     * The threshold value must be positive and not exceed MaxRcvWnd.
     */
    void setWindowUpdateThreshold (TcpSeqInt rcv_ann_thres)
    {
        assert_started();
        AIPSTACK_ASSERT(rcv_ann_thres > 0)
        AIPSTACK_ASSERT(rcv_ann_thres <= TcpConConstants::MaxWindow)
        
        m_v.rcv_ann_thres = rcv_ann_thres;
    }
    
    /**
     * Set the window update threshold as a proportion of the expected
     * receive buffer size.
     * 
     * The 'div' defines the proportion as a division of buffer_size, it must
     * be greater than or equal to 2.
     */
    void setProportionalWindowUpdateThreshold (std::size_t buffer_size, int div)
    {
        AIPSTACK_ASSERT(div >= 2)
        using UInt = unsigned int;
        
        TcpSeqInt max_rx_window = MinValueU(buffer_size, TcpApi<Arg>::MaxRcvWnd);
        TcpSeqInt thres = MaxValue(TcpSeqInt(1), TcpSeqInt(max_rx_window / UInt(div)));
        
        setWindowUpdateThreshold(thres);
    }
    
    /**
     * Returns the last announced receive window.
     * May only be called in CONNECTED state.
     * This is intended to be used when a connection is accepted to determine
     * the minimum amount of receive buffer which must be available.
     */
    std::size_t getAnnouncedRcvWnd () const
    {
        assert_connected();
        
        TcpSeqInt ann_wnd = m_v.pcb->rcv_ann_wnd;
        
        // In SYN_SENT we subtract one because one was added
        // by create_connection for receiving the SYN.
        if (m_v.pcb->state() == TcpStates::SYN_SENT) {
            AIPSTACK_ASSERT(ann_wnd > 0)
            ann_wnd -= 1u;
        }
        
        AIPSTACK_ASSERT(ann_wnd <= TypeMax<std::size_t>)
        return std::size_t(ann_wnd);
    }
    
    /**
     * Sets the receive buffer.
     * Typically the application will call this once just after a connection
     * is established.
     * May only be called in CONNECTED or CLOSED state.
     * If a receive buffer has already been set than the new buffer must be
     * at least as large as the old one and the leading portion corresponding
     * to the old size must have been copied (because it may contain buffered
     * out-of-sequence data).
     */
    void setRecvBuf (IpBufRef rcv_buf)
    {
        assert_started();
        AIPSTACK_ASSERT(rcv_buf.tot_len >= m_v.rcv_buf.tot_len)
        
        // Set the receive buffer.
        m_v.rcv_buf = rcv_buf;
        
        if (m_v.pcb != nullptr) {
            // Inform the input code, so it can send a window update.
            TcpConInput::pcb_rcv_buf_extended(m_v.pcb);
        }
    }
    
    /**
     * Extends the receive buffer for the specified amount.
     * May only be called in CONNECTED or CLOSED state.
     */
    void extendRecvBuf (std::size_t amount)
    {
        assert_started();
        AIPSTACK_ASSERT(amount <= TypeMax<std::size_t> - m_v.rcv_buf.tot_len)
        
        // Extend the receive buffer.
        m_v.rcv_buf.tot_len += amount;
        
        if (m_v.pcb != nullptr) {
            // Inform the input code, so it can send a window update.
            TcpConInput::pcb_rcv_buf_extended(m_v.pcb);
        }
    }
    
    /**
     * Returns the current receive buffer.
     * May only be called in CONNECTED or CLOSED state.
     * 
     * When the stack shifts the receive buffer it will move to
     * subsequent buffer nodes eagerly (see IpBufRef::processBytes).
     * This is convenient when using a ring buffer as it guarantees
     * that the offset will remain less than the buffer size.
     */
    inline IpBufRef getRecvBuf () const
    {
        assert_started();
        
        return m_v.rcv_buf;
    }
    
    /**
     * Returns whether a FIN was received.
     * May only be called in CONNECTED or CLOSED state.
     */
    inline bool wasEndReceived () const
    {
        assert_started();
        
        return m_v.end_received;
    }
    
    /**
     * Returns the amount of send buffer that could remain unsent
     * indefinitely in the absence of sendPush or endSending.
     * 
     * For accepted connections, this does not change, and for
     * initiated connections, it only possibly decreases when the
     * connection is established.
     */
    inline std::size_t getSndBufOverhead () const
    {
        assert_connected();
        
        // Sending can be delayed for segmentation only when we have
        // less than the MSS data left to send, hence return mss-1.
        return m_v.pcb->base_snd_mss - 1;
    }
    
    /**
     * Sets the send buffer.
     * Typically the application will call this once just after a connection
     * is established.
     * May only be called in CONNECTED or CLOSED state.
     * May only be called before endSending is called.
     * If a send buffer has already been set than the new buffer must be
     * at least as large as the old one and the leading portion corresponding
     * to the old size must have been copied (because some of the data may have
     * been sent).
     */
    void setSendBuf (IpBufRef snd_buf)
    {
        assert_sending();
        AIPSTACK_ASSERT(snd_buf.tot_len >= m_v.snd_buf.tot_len)
        AIPSTACK_ASSERT(m_v.snd_buf_cur.tot_len <= m_v.snd_buf.tot_len)
        
        // Calculate the send offset and check if the send buffer is being extended.
        std::size_t snd_offset = m_v.snd_buf.tot_len - m_v.snd_buf_cur.tot_len;
        bool extended = snd_buf.tot_len > m_v.snd_buf.tot_len;

        // Set the send buffer.
        m_v.snd_buf = snd_buf;
        
        // Set snd_buf_cur and advance it to preserve the send offset.
        m_v.snd_buf_cur = snd_buf;
        m_v.snd_buf_cur.skipBytes(snd_offset);
        
        if (AIPSTACK_LIKELY(m_v.pcb != nullptr && extended)) {
            // Inform the output code, so it may send the data.
            TcpConOutput::pcb_snd_buf_extended(m_v.pcb);
        }
    }
    
    /**
     * Extends the send buffer for the specified amount.
     * May only be called in CONNECTED or CLOSED state.
     * May only be called before endSending is called.
     */
    void extendSendBuf (std::size_t amount)
    {
        assert_sending();
        AIPSTACK_ASSERT(amount <= TypeMax<std::size_t> - m_v.snd_buf.tot_len)
        AIPSTACK_ASSERT(m_v.snd_buf_cur.tot_len <= m_v.snd_buf.tot_len)
        
        // Increment the amount of data in the send buffer.
        m_v.snd_buf.tot_len += amount;
        
        // Also adjust snd_buf_cur.
        m_v.snd_buf_cur.tot_len += amount;
    
        if (AIPSTACK_LIKELY(m_v.pcb != nullptr && amount > 0)) {
            // Inform the output code, so it may send the data.
            TcpConOutput::pcb_snd_buf_extended(m_v.pcb);
        }
    }
    
    /**
     * Returns the current send buffer.
     * May only be called in CONNECTED or CLOSED state.
     * 
     * When the stack shifts the send buffer it will move to
     * subsequent buffer nodes eagerly (see IpBufRef::processBytes).
     * This is convenient when using a ring buffer as it guarantees
     * that the offset will remain less than the buffer size.
     */
    inline IpBufRef getSendBuf () const
    {
        assert_started();
        
        return m_v.snd_buf;
    }
    
    /**
     * Indicates the end of the data stream, i.e. queues a FIN.
     * May only be called in CONNECTED or CLOSED state.
     * May only be called before endSending is called.
     */
    void closeSending ()
    {
        assert_sending();
        
        // Set the push index to the end of the send buffer.
        m_v.snd_psh_index = m_v.snd_buf.tot_len;
        
        // Remember that sending is closed.
        m_v.snd_closed = true;
        
        // Inform the output code, e.g. to adjust the PCB state
        // and send a FIN. But not in SYN_SENT, in that case the
        // input code will take care of it when the SYN is received.
        if (m_v.pcb != nullptr && m_v.pcb->state() != TcpStates::SYN_SENT) {
            TcpConOutput::pcb_end_sending(m_v.pcb);
        }
    }
    
    /**
     * Returns whethercloseSending has been called.
     * May only be called in CONNECTED or CLOSED state.
     */
    inline bool wasSendingClosed () const
    {
        assert_started();
        
        return m_v.snd_closed;
    }
    
    /**
     * Returns whether a FIN was sent and acknowledged.
     * May only be called in CONNECTED or CLOSED state.
     */
    inline bool wasEndSent () const
    {
        assert_started();
        
        return m_v.end_sent;
    }
    
    /**
     * Push sending of currently queued data.
     * May only be called in CONNECTED or CLOSED state.
     */
    void sendPush ()
    {
        assert_started();
        
        // No need to do anything after closeSending.
        if (m_v.snd_closed) {
            return;
        }
        
        // Set the push index to the current send buffer size.
        m_v.snd_psh_index = m_v.snd_buf.tot_len;
        
        // Tell the output code to push, if necessary.
        if (m_v.pcb != nullptr && m_v.pcb->state().isSndOpen() &&
            m_v.snd_buf.tot_len > 0)
        {
            TcpConOutput::pcb_push_output(m_v.pcb);
        }
    }
    
protected:
    /**
     * Deinitializes the connection object.
     * Note that destruction performs a reset with have_unprocessed_data=false. It may
     * be a good idea to first call reset with the correct have_unprocessed_data value
     * (see reset documentation).
     * 
     * This destructor is intentionally not virtual but is protected to prevent
     * incorrect usage.
     */
    ~TcpConnection ()
    {
        reset();
    }
    
    /**
     * Called when the connection is aborted.
     * This callback corresponds to a transition from CONNECTED
     * to CLOSED state, which happens just before the callback.
     */
    virtual void connectionAborted () = 0;
    
    /**
     * Called for actively opened connections when the connection
     * is established.
     */
    virtual void connectionEstablished () {}
    
    /**
     * Called when some data or FIN has been received.
     * 
     * Each callback corresponds to shifting of the receive
     * buffer by that amount. Zero amount indicates that FIN
     * was received.
     */
    virtual void dataReceived (std::size_t amount) = 0;
    
    /**
     * Called when some data or FIN has been sent and acknowledged.
     * 
     * Each dataSent callback corresponds to shifting of the send buffer
     * by that amount. Zero amount indicates that FIN was acknowledged.
     */
    virtual void dataSent (std::size_t amount) = 0;
    
private:
    inline IpMtuRef<TcpConStackArg> & mtu_ref () {
        return *this;
    }

    void assert_init () const
    {
        AIPSTACK_ASSERT(!m_v.started && !m_v.snd_closed &&
                        !m_v.end_sent && !m_v.end_received)
        AIPSTACK_ASSERT(m_v.pcb == nullptr)
    }
    
    void assert_started () const
    {
        AIPSTACK_ASSERT(m_v.started)
        AIPSTACK_ASSERT(m_v.pcb == nullptr ||
                        m_v.pcb->state() == TcpStates::SYN_SENT ||
                        m_v.pcb->state().isActive())
        AIPSTACK_ASSERT(m_v.pcb == nullptr || m_v.pcb->con == this)
        AIPSTACK_ASSERT(m_v.pcb == nullptr ||
                        m_v.pcb->state() == TcpStates::SYN_SENT ||
                        m_v.pcb->state().isSndOpen() == !m_v.snd_closed)
    }
    
    void assert_connected () const
    {
        assert_started();
        AIPSTACK_ASSERT(m_v.pcb != nullptr)
    }
    
    void assert_sending () const
    {
        assert_started();
        AIPSTACK_ASSERT(!m_v.snd_closed)
    }
    
    void setup_common_started ()
    {
        // Clear buffer variables.
        m_v.snd_buf = IpBufRef{};
        m_v.rcv_buf = IpBufRef{};
        m_v.snd_buf_cur = IpBufRef{};
        m_v.snd_psh_index = 0;
        
        // Initialize rcv_ann_thres.
        m_v.rcv_ann_thres = TcpConConstants::DefaultWndAnnThreshold;
        
        // Initialize the out-of-sequence information.
        m_v.ooseq.init();
        
        // Set STARTED flag to indicate we're no longer in INIT state.
        m_v.started = true;
    }
    
private:
    // These are called by TCP internals when various things happen.
    
    // NOTE: This does not add the PCB to the unreferenced list.
    // It will be done afterward by the caller. This makes sure that
    // any allocate_pcb done from the user callback will not find
    // this PCB.
    void pcb_aborted ()
    {
        assert_connected();
        
        TcpConPcb *pcb = m_v.pcb;
        
        // Reset the MtuRef.
        mtu_ref().reset(pcb->tcp->m_stack);
        
        // Disassociate with the PCB.
        pcb->con = nullptr;
        m_v.pcb = nullptr;
        
        // Call the application callback.
        connectionAborted();
    }
    
    void connection_established ()
    {
        assert_connected();
        
        // Call the application callback.
        connectionEstablished();
    }
    
    void data_sent (std::size_t amount)
    {
        assert_connected();
        AIPSTACK_ASSERT(!m_v.end_sent)
        AIPSTACK_ASSERT(amount > 0)
        
        // Call the application callback.
        dataSent(amount);
    }
    
    void end_sent ()
    {
        assert_connected();
        AIPSTACK_ASSERT(!m_v.end_sent)
        AIPSTACK_ASSERT(m_v.snd_closed)
        
        // Remember that end was sent.
        m_v.end_sent = true;
        
        // Call the application callback.
        dataSent(0);
    }
    
    void data_received (std::size_t amount)
    {
        assert_connected();
        AIPSTACK_ASSERT(!m_v.end_received)
        AIPSTACK_ASSERT(amount > 0)
        
        // Call the application callback.
        dataReceived(amount);
    }
    
    void end_received ()
    {
        assert_connected();
        AIPSTACK_ASSERT(!m_v.end_received)
        
        // Remember that end was received.
        m_v.end_received = true;
        
        // Call the application callback.
        dataReceived(0);
    }
    
    // Callback from MtuRef when the PMTU changes.
    void pmtuChanged (std::uint16_t pmtu) override final
    {
        assert_connected();
        
        TcpConOutput::pcb_pmtu_changed(m_v.pcb, pmtu);
    }
    
    void reset_flags ()
    {
        m_v.started      = false;
        m_v.snd_closed   = false;
        m_v.end_sent     = false;
        m_v.end_received = false;
    }
    
private:
    struct TcpConVars {
        TcpConPcb *pcb;
        IpBufRef snd_buf;
        IpBufRef rcv_buf;
        IpBufRef snd_buf_cur;
        TcpSeqInt snd_wnd : 30;
        TcpSeqInt started : 1;
        TcpSeqInt snd_closed : 1;
        TcpSeqInt cwnd;
        TcpSeqInt ssthresh;
        TcpSeqInt cwnd_acked;
        TcpSeqNum recover;
        TcpSeqInt rcv_ann_thres : 30;
        TcpSeqInt end_sent : 1;
        TcpSeqInt end_received : 1;
        TcpSeqNum rtt_test_seq;
        typename TcpConConstants::RttType rttvar;
        typename TcpConConstants::RttType srtt;
        TcpConOosBuffer ooseq;
        std::size_t snd_psh_index;
    };
    
    TcpConVars m_v;
};

}

#endif
