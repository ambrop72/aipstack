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

#ifndef AIPSTACK_IP_MTU_REF_H
#define AIPSTACK_IP_MTU_REF_H

#include <stdint.h>

#include <aipstack/misc/Use.h>
#include <aipstack/ip/IpAddr.h>

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
 * Allows keeping track of the Path MTU estimate for a remote address.
 * 
 * @tparam Arg Template parameter of @ref IpStack.
 */
template <typename Arg>
class IpMtuRef
#ifndef IN_DOXYGEN
    : private IpStack<Arg>::BaseMtuRef
#endif
{
    AIPSTACK_USE_TYPES(IpStack<Arg>, (BaseMtuRef, PathMtuCache))

public:
    /**
     * Construct the MTU reference.
     * 
     * The object is constructed in not-setup state, that is without an
     * associated remote address. To set the remote address, call @ref setup.
     * This function must be called before any other function in this
     * class is called.
     */
    IpMtuRef () = default;
    
    /**
     * Reset the MTU reference.
     * 
     * This resets the object to the not-setup state.
     *
     * @note
     * It is required to reset the object to not-setup state
     * before destructing it, if not already in not-setup state.
     * 
     * @param stack The IP stack.
     */
    inline void reset (IpStack<Arg> *stack)
    {
        return BaseMtuRef::reset(mtu_cache(stack));
    }
    
    /**
     * Check if the MTU reference is in setup state.
     * 
     * @return True if in setup state, false if in not-setup state.
     */
    inline bool isSetup () const
    {
        return BaseMtuRef::isSetup();
    }
    
    /**
     * Setup the MTU reference for a specific remote address.
     * 
     * The object must be in not-setup state when this is called.
     * On success, the current PMTU estimate is provided and future PMTU
     * estimate changes will be reported via the @ref pmtuChanged callback.
     * 
     * @warning
     * Do not destruct the object while it is in setup state.
     * First use @ref reset (or @ref moveFrom) to change the object to
     * not-setup state before destruction.
     * 
     * @param stack The IP stack.
     * @param remote_addr The remote address to observe the PMTU for.
     * @param iface NULL or the interface though which remote_addr would be
     *        routed, as an optimization.
     * @param out_pmtu On success, will be set to the current PMTU estimate
     *        (guaranteed to be at least MinMTU). On failure it will not be
     *        changed.
     * @return True on success (object enters setup state), false on failure
     *         (object remains in not-setup state).
     */
    inline bool setup (IpStack<Arg> *stack, Ip4Addr remote_addr, IpIface<Arg> *iface,
                       uint16_t &out_pmtu)
    {
        return BaseMtuRef::setup(mtu_cache(stack), remote_addr, iface, out_pmtu);
    }

    /**
     * Move an MTU reference from another object to this one.
     * 
     * This object must be in not-setup state. Upon return, the 'src' object
     * will be in not-setup state and this object will be in whatever state
     * the 'src' object was. If the 'src' object was in setup state, this object
     * will be setup with the same remote address.
     * 
     * @param src The object to move from.
     */
    inline void moveFrom (IpMtuRef &src)
    {
        return BaseMtuRef::moveFrom(src);
    }
    
protected:
    /**
     * Destruct the MTU reference, asserting not-setup state.
     * 
     * @warning
     * It is required to ensure the object is in not-setup state before
     * destructing it (by calling @ref reset if needed). The destructor
     * cannot do the reset itself because it does not have the @ref IpStack
     * pointer available (to avoid using additional memory).
     * 
     * This destructor is intentionally not virtual but is protected to prevent
     * incorrect usage.
     */
    ~IpMtuRef () = default;
    
    /**
     * Callback which reports changes of the PMTU estimate.
     * 
     * This is called whenever the PMTU estimate changes,
     * and only in setup state.
     * 
     * WARNING: Do not change this object in any way from this callback,
     * specifically do not call @ref reset or @ref moveFrom. Note that the
     * implementation calls all these callbacks for the same remote address
     * in a loop, and that the callbacks may be called from within
     * @ref IpStack::handleIcmpPacketTooBig and @ref IpStack::handleLocalPacketTooBig.
     * 
     * @param pmtu The new PMTU estimate (guaranteed to be at least MinMTU).
     */
    virtual void pmtuChanged (uint16_t pmtu) = 0;
    
private:
    inline static PathMtuCache * mtu_cache (IpStack<Arg> *stack)
    {
        return &stack->m_path_mtu_cache;
    }
};

/** @} */

}

#endif
