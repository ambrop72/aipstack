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

#ifndef AIPSTACK_TCP_LISTEN_QUEUE_H
#define AIPSTACK_TCP_LISTEN_QUEUE_H

#include <cstddef>

#include <aipstack/misc/Use.h>
#include <aipstack/misc/Assert.h>
#include <aipstack/misc/NonCopyable.h>
#include <aipstack/misc/Function.h>
#include <aipstack/infra/Buf.h>
#include <aipstack/infra/Err.h>
#include <aipstack/tcp/TcpApi.h>
#include <aipstack/tcp/TcpConnection.h>
#include <aipstack/tcp/TcpListener.h>
#include <aipstack/platform/PlatformFacade.h>

namespace AIpStack {

template <
    typename PlatformImpl,
    typename TcpArg,
    std::size_t RxBufferSize
>
class TcpListenQueue {
    using Platform = PlatformFacade<PlatformImpl>;
    AIPSTACK_USE_TYPES(Platform, (TimeType))
    
    using Connection = TcpConnection<TcpArg>;
    using Listener = TcpListener<TcpArg>;

    static_assert(RxBufferSize > 0, "");
    
public:
    class QueuedListener;
    
    class ListenQueueEntry :
        private Connection
    {
        friend class QueuedListener;
        
    private:
        void init (QueuedListener *listener)
        {
            m_listener = listener;
            m_rx_buf_node = IpBufNode{m_rx_buf, RxBufferSize, nullptr};
        }
        
        void deinit ()
        {
            Connection::reset();
        }
        
        void accept_connection ()
        {
            AIPSTACK_ASSERT(Connection::isInit())
            AIPSTACK_ASSERT(m_listener->m_queue_size > 0)
            
            if (Connection::acceptConnection(m_listener->m_listener) != IpErr::SUCCESS) {
                return;
            }
            
            Connection::setRecvBuf(IpBufRef{&m_rx_buf_node, 0, RxBufferSize});
            
            m_time = Connection::getApi().platform().getTime();
            m_ready = false;
            
            // Added a not-ready connection -> update timeout.
            m_listener->update_timeout();
        }
        
        void reset_connection ()
        {
            AIPSTACK_ASSERT(!Connection::isInit())
            
            Connection::reset();
            
            if (!m_ready) {
                // Removed a not-ready connection -> update timeout.
                m_listener->update_timeout();
            }
        }
        
        IpBufRef get_received_data ()
        {
            AIPSTACK_ASSERT(!Connection::isInit())
            
            std::size_t rx_buf_len = Connection::getRecvBuf().tot_len;
            AIPSTACK_ASSERT(rx_buf_len <= RxBufferSize)
            std::size_t rx_len = RxBufferSize - rx_buf_len;
            return IpBufRef{&m_rx_buf_node, 0, rx_len};
        }
        
    private:
        void connectionAborted () override final
        {
            AIPSTACK_ASSERT(!Connection::isInit())
            
            reset_connection();
        }
        
        void dataReceived (std::size_t amount) override final
        {
            AIPSTACK_ASSERT(!Connection::isInit())
            
            // If we get a FIN without any data, abandon the connection.
            if (amount == 0 && Connection::getRecvBuf().tot_len == RxBufferSize) {
                reset_connection();
                return;
            }
            
            if (!m_ready) {
                // Some data has been received, connection is now ready.
                m_ready = true;
                
                // Non-ready connection changed to ready -> update timeout.
                m_listener->update_timeout();
                
                // Try to hand over ready connections.
                m_listener->dispatch_connections();
            }
        }
        
        void dataSent (std::size_t) override final
        {
            AIPSTACK_ASSERT(false) // nothing was sent so this cannot be called
        }
        
    private:
        QueuedListener *m_listener;
        TimeType m_time;
        IpBufNode m_rx_buf_node;
        bool m_ready;
        char m_rx_buf[RxBufferSize];
    };
    
    struct ListenQueueParams {
        std::size_t min_rcv_buf_size = 0;
        int queue_size = 0;
        TimeType queue_timeout = 0;
        ListenQueueEntry *queue_entries = nullptr;
    };
    
public:
    class QueuedListener :
        private NonCopyable<QueuedListener>
    {
        friend class ListenQueueEntry;
        
    public:
        using EstablishedHandler = Function<void()>;

        QueuedListener (
            PlatformFacade<PlatformImpl> platform, EstablishedHandler established_handler)
        :
            m_established_handler(established_handler),
            m_listener(
                AIPSTACK_BIND_MEMBER_TN(&QueuedListener::establishedHandler, this)),
            m_dequeue_timer(platform,
                AIPSTACK_BIND_MEMBER_TN(&QueuedListener::dequeueTimerHandler, this)),
            m_timeout_timer(platform,
                AIPSTACK_BIND_MEMBER_TN(&QueuedListener::timeoutTimerHandler, this))
        {}
        
        ~QueuedListener ()
        {
            deinit_queue();
        }
        
        void reset ()
        {
            deinit_queue();
            m_dequeue_timer.unset();
            m_timeout_timer.unset();
            m_listener.reset();
        }
        
        bool startListening (TcpApi<TcpArg> &tcp, TcpListenParams const &params, ListenQueueParams const &q_params)
        {
            AIPSTACK_ASSERT(!m_listener.isListening())
            AIPSTACK_ASSERT(q_params.queue_size >= 0)
            AIPSTACK_ASSERT(q_params.queue_size == 0 || q_params.queue_entries != nullptr)
            AIPSTACK_ASSERT(q_params.queue_size == 0 || q_params.min_rcv_buf_size >= RxBufferSize)
            
            // Start listening.
            if (!m_listener.startListening(tcp, params)) {
                return false;
            }
            
            // Init queue variables.
            m_queue = q_params.queue_entries;
            m_queue_size = q_params.queue_size;
            m_queue_timeout = q_params.queue_timeout;
            m_queued_to_accept = nullptr;
            
            // Init queue entries.
            for (int i = 0; i < m_queue_size; i++) {
                m_queue[i].init(this);
            }
            
            // Set the initial receive window.
            std::size_t initial_rx_window = (m_queue_size == 0) ? q_params.min_rcv_buf_size : RxBufferSize;
            m_listener.setInitialReceiveWindow(initial_rx_window);
            
            return true;
        }
        
        void scheduleDequeue ()
        {
            AIPSTACK_ASSERT(m_listener.isListening())
            
            if (m_queue_size > 0) {
                m_dequeue_timer.setNow();
            }
        }
        
        // NOTE: If m_queue_size>0, there are complications that you
        // must deal with:
        // - Any initial data which has already been received will be returned
        //   in initial_rx_data. You must copy this data immediately after this
        //   function returns and process it correctly.
        // - You must also immediately copy the contents of the existing remaining
        //   receive buffer (getRecvBuf) to your own receive buffer before calling
        //   setRecvBuf to set your receive buffer. This is because out-of-sequence
        //   data may have been stored there.
        // - A FIN may already have been received. If so you will not get a
        //   dataReceived(0) callback.
        IpErr acceptConnection (TcpConnection<TcpArg> &dst_con, IpBufRef &initial_rx_data)
        {
            AIPSTACK_ASSERT(m_listener.isListening())
            AIPSTACK_ASSERT(dst_con.isInit())
            
            if (m_queue_size == 0) {
                AIPSTACK_ASSERT(m_listener.hasAcceptPending())
                
                initial_rx_data = IpBufRef{};
                return dst_con.acceptConnection(m_listener);
            } else {
                AIPSTACK_ASSERT(m_queued_to_accept != nullptr)
                AIPSTACK_ASSERT(!m_queued_to_accept->Connection::isInit())
                AIPSTACK_ASSERT(m_queued_to_accept->m_ready)
                
                ListenQueueEntry *entry = m_queued_to_accept;
                m_queued_to_accept = nullptr;
                
                initial_rx_data = entry->get_received_data();
                dst_con.moveConnection(entry);
                return IpErr::SUCCESS;
            }
        }
        
    private:
        void establishedHandler ()
        {
            AIPSTACK_ASSERT(m_listener.isListening())
            AIPSTACK_ASSERT(m_listener.hasAcceptPending())
            
            if (m_queue_size == 0) {
                // Call the accept callback so the user can call acceptConnection.
                m_established_handler();
            } else {
                // Try to accept the connection into the queue.
                for (int i = 0; i < m_queue_size; i++) {
                    ListenQueueEntry &entry = m_queue[i];
                    if (entry.Connection::isInit()) {
                        entry.accept_connection();
                        break;
                    }
                }
            }
            
            // If the connection was not accepted, it will be aborted.
        }
        
        void dequeueTimerHandler ()
        {
            dispatch_connections();
        }
        
        void dispatch_connections ()
        {
            AIPSTACK_ASSERT(m_listener.isListening())
            AIPSTACK_ASSERT(m_queue_size > 0)
            AIPSTACK_ASSERT(m_queued_to_accept == nullptr)
            
            // Try to dispatch the oldest ready connections.
            while (ListenQueueEntry *entry = find_oldest(true)) {
                AIPSTACK_ASSERT(!entry->Connection::isInit())
                AIPSTACK_ASSERT(entry->m_ready)
                
                // Call the accept handler, while publishing the connection.
                m_queued_to_accept = entry;
                m_established_handler();
                m_queued_to_accept = nullptr;
                
                // If the connection was not taken, stop trying.
                if (!entry->Connection::isInit()) {
                    break;
                }
            }
        }
        
        void update_timeout ()
        {
            AIPSTACK_ASSERT(m_listener.isListening())
            AIPSTACK_ASSERT(m_queue_size > 0)
            
            ListenQueueEntry *entry = find_oldest(false);
            
            if (entry != nullptr) {
                TimeType expire_time = entry->m_time + m_queue_timeout;
                m_timeout_timer.setAt(expire_time);
            } else {
                m_timeout_timer.unset();
            }
        }
        
        void timeoutTimerHandler ()
        {
            AIPSTACK_ASSERT(m_listener.isListening())
            AIPSTACK_ASSERT(m_queue_size > 0)
            
            // We must have a non-ready connection since we keep the timeout
            // always updated to expire for the oldest non-ready connection
            // (or not expire if there is none).
            ListenQueueEntry *entry = find_oldest(false);
            AIPSTACK_ASSERT(entry != nullptr)
            AIPSTACK_ASSERT(!entry->Connection::isInit())
            AIPSTACK_ASSERT(!entry->m_ready)
            
            // Reset the oldest non-ready connection.
            entry->reset_connection();
        }
        
        void deinit_queue ()
        {
            if (m_listener.isListening()) {
                for (int i = 0; i < m_queue_size; i++) {
                    m_queue[i].deinit();
                }
            }
        }
        
        ListenQueueEntry * find_oldest (bool ready)
        {
            ListenQueueEntry *oldest_entry = nullptr;
            
            for (int i = 0; i < m_queue_size; i++) {
                ListenQueueEntry &entry = m_queue[i];
                if (!entry.Connection::isInit() && entry.m_ready == ready &&
                    (oldest_entry == nullptr ||
                     !Platform::timeGreaterOrEqual(entry.m_time, oldest_entry->m_time)))
                {
                    oldest_entry = &entry;
                }
            }
            
            return oldest_entry;
        }
        
    private:
        EstablishedHandler m_established_handler;
        Listener m_listener;
        typename Platform::Timer m_dequeue_timer;
        typename Platform::Timer m_timeout_timer;
        ListenQueueEntry *m_queue;
        int m_queue_size;
        TimeType m_queue_timeout;
        ListenQueueEntry *m_queued_to_accept;
    };
};

}

#endif
