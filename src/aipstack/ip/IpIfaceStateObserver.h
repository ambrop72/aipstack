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

#ifndef AIPSTACK_IP_IFACE_STATE_OBSERVER_H
#define AIPSTACK_IP_IFACE_STATE_OBSERVER_H

#include <aipstack/misc/Function.h>
#include <aipstack/infra/ObserverNotification.h>

namespace AIpStack {

#ifndef IN_DOXYGEN
template<typename> class IpStack;
template<typename> class IpIface;
template<typename> class IpDriverIface;
#endif

/**
 * @addtogroup ip-stack
 * @{
 */

/**
 * Allows observing changes in the driver-reported state of an interface.
 * 
 * The driver-reported state can be queried using by @ref IpIface::getDriverState.
 * This class can be used to receive a callback whenever the driver-reported
 * state may have changed.
 * 
 * This class is based on @ref Observer and the functionality of
 * of that class is exposed. The specific @ref observe function is provided to
 * start observing an interface.
 * 
 * @tparam Arg Template parameter of @ref IpStack.
 */
template<typename Arg>
class IpIfaceStateObserver :
    public Observer<IpIfaceStateObserver<Arg>>
{
    template<typename> friend class IpDriverIface;
    friend Observable<IpIfaceStateObserver>;
    
public:
    /**
     * Type of callback called when the driver-reported state of the interface
     * may have changed.
     * 
     * It is not guaranteed that the state has actually changed, nor is it
     * guaranteed that the callback will be called immediately for every state
     * change (there may be just one callback for successive state changes).
     * 
     * WARNING: The callback must not do any potentially harmful actions such
     * as removing the interface. Removing this or other listeners and adding
     * other listeners is safe. Sending packets should be safe assuming this
     * is safe in the driver.
     */
    using StateChangedHandler = Function<void()>;

    /**
     * Construct the state observer.
     * 
     * @param handler Callback function used to report state changes (must not
     *        be null).
     */
    IpIfaceStateObserver (StateChangedHandler handler) :
        m_handler(handler)
    {}

    /**
     * Start observing an interface, making the observer active.
     * 
     * The observer must be inactive when this is called.
     * 
     * @param iface Interface to observe.
     */
    inline void observe (IpIface<Arg> &iface)
    {
        iface.m_state_observable.addObserver(*this);
    }
    
private:
    StateChangedHandler m_handler;
};

/** @} */

}

#endif
