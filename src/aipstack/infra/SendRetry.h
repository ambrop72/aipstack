/*
 * Copyright (c) 2016 Ambroz Bizjak
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

#ifndef AIPSTACK_SEND_RETRY_H
#define AIPSTACK_SEND_RETRY_H

#include <aipstack/infra/ObserverNotification.h>

namespace AIpStack {

/**
 * @ingroup infra
 * @defgroup send-retry Send-Retry Mechanism
 * @brief Mechanism to notify senders when to retry sending a packet.
 * 
 * This module provides a generic mechanism for a packet sender to be notified when to
 * retry a failed send attempt. The reason for send failure could generally be anything, but
 * most commonly it will be a pending ARP query or a buffer being full.
 * 
 * When sending a packet, the sender passes a pointer to an @ref IpSendRetryRequest object
 * to some kind of send function. If sending fails and retry notification is supported for
 * the specific failure cause, the send function (itself or a lower-layer function) calls
 * @ref IpSendRetryList::addRequest to add the sender to a list of senders interested in
 * retry notification. Later, the same module which manages that @ref IpSendRetryList may
 * call @ref IpSendRetryList::dispatchRequests, which would notify the senders which have
 * been added to the list.
 * 
 * The send-retry mechanism should be used as a hint only and must not be relied upon, as
 * there is generally no guarantee that this mechanism is supported, and even where it is
 * there is no guarantee that a notification will actually be generated in all possible
 * circumstances.
 * 
 * The @ref IpSendRetryRequest and @ref IpSendRetryList are not copy/move-constructible or
 * copy/move-assignable by design.
 * 
 * The implementation is based on the @ref observer mechanism and it essentially a simple
 * wrapper around that.
 * 
 * @{
 */

class IpSendRetryList;

/**
 * Represents a request to be notified when a failed send attempt should be retried.
 * 
 * See the @ref send-retry module description for an explanation of the send-retry
 * mechanism.
 * 
 * A request object is either unassociated or associated with a specific
 * @ref IpSendRetryList. Association is established using @ref IpSendRetryList::addRequest.
 */
class IpSendRetryRequest
#ifndef IN_DOXYGEN
    :private Observer<IpSendRetryRequest>
#endif
{
    using SendRetryBaseObserver = Observer<IpSendRetryRequest>;
    friend class IpSendRetryList;
    friend Observable<IpSendRetryRequest>;
    
public:
    /**
     * Construct an unassociated request.
     */
    IpSendRetryRequest () = default;
    
    /**
     * Return whether the request is associated.
     * 
     * @return True if associated, false if unassociated.
     */
    inline bool isActive () const
    {
        return SendRetryBaseObserver::isActive();
    }
    
    /**
     * Disassociate the request if associated.
     */
    inline void reset ()
    {
        SendRetryBaseObserver::reset();
    }

protected:
    /**
     * Destruct the request, disassociating it if associated.
     * 
     * This destructor is intentionally not virtual but is protected to prevent
     * incorrect usage.
     */
    ~IpSendRetryRequest () = default;
    
    /**
     * Callback called when sending should be retried, from
     * @ref IpSendRetryList::dispatchRequests.
     * 
     * This callback is called only when the request is associated, and the request
     * is disassociated just prior to the call. It is allowed to associate and disassociate
     * this or other requests to/from the same or other @ref IpSendRetryList. However, it is
     * not allowed to destruct the calling @ref IpSendRetryList from this callback.
     * 
     * The specific implementation of @ref IpSendRetryList may impose additional
     * restrictions. Note that the restriction to not destruct the @ref IpSendRetryList
     * generally implies the restriction to not destruct the object managing it.
     */
    virtual void retrySending () = 0;
};

/**
 * Represents a list of failed send attempts which can be notified all at once.
 * 
 * See the @ref send-retry module description for an explanation of the send-retry
 * mechanism.
 * 
 * A retry list has an associated set of requests (@ref IpSendRetryRequest instances).
 * Requests can be associated and disassociated dynamically.
 */
class IpSendRetryList
#ifndef IN_DOXYGEN
    :private Observable<IpSendRetryRequest>
#endif
{
    using BaseObservable = Observable<IpSendRetryRequest>;
    
public:
    /**
     * Construct a retry list with no associated requests.
     */
    IpSendRetryList () = default;
    
    /**
     * Destruct the retry list, disassociating any requests.
     */
    ~IpSendRetryList () = default;
    
    /**
     * Return if the retry list has any associated requests.
     * 
     * @return True if there is at least one associated request, false if none.
     */
    inline bool hasRequests () const
    {
        return BaseObservable::hasObservers();
    }
    
    /**
     * Disassociate any requests from this retry list.
     * 
     * Any requests which were associated with this retry list become unassociated.
     */
    inline void reset ()
    {
        BaseObservable::reset();
    }
    
    /**
     * Associate a request with this retry list.
     * 
     * @param req The request to associate, or null (in that case nothing is done).
     *        If the request is already associated, it is first disassociated.
     */
    void addRequest (IpSendRetryRequest *req)
    {
        if (req != nullptr) {
            req->SendRetryBaseObserver::reset();
            BaseObservable::addObserver(*req);
        }
    }
    
    /**
     * Notify the requests associated with this retry list while removing them.
     * 
     * This calls the @ref IpSendRetryRequest::retrySending callback for each request,
     * disassociating each request from the retry list just before its call. The order of
     * notifications is not specified.
     */
    void dispatchRequests ()
    {
        BaseObservable::notifyRemoveObservers([&](IpSendRetryRequest &request) {
            request.retrySending();
        });
    }
};

/** @} */

}

#endif
