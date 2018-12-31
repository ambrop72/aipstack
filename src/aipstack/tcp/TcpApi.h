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

#ifndef AIPSTACK_TCP_API_H
#define AIPSTACK_TCP_API_H

#include <aipstack/misc/NonCopyable.h>
#include <aipstack/platform/PlatformFacade.h>
#include <aipstack/tcp/TcpSeqNum.h>
#include <aipstack/tcp/TcpListener.h>
#include <aipstack/tcp/TcpConnection.h>
#include <aipstack/tcp/IpTcpProto_constants.h>

namespace AIpStack {

#ifndef IN_DOXYGEN
template <typename> class IpTcpProto;
#endif

template <typename Arg>
class TcpApi :
    private NonCopyable<TcpApi<Arg>>
{
    template <typename> friend class IpTcpProto;
    template <typename> friend class TcpListener;
    template <typename> friend class TcpConnection;

    using Constants = IpTcpProto_constants<Arg>;

private:
    inline IpTcpProto<Arg> & proto () {
        return static_cast<IpTcpProto<Arg> &>(*this);
    }

    inline IpTcpProto<Arg> const & proto () const {
        return static_cast<IpTcpProto<Arg> const &>(*this);
    }

    // Prevent construction except from IpTcpProto (which is a friend). The second
    // declaration disables aggregate construction.
    TcpApi () = default;
    TcpApi (int);

public:
    using Listener = TcpListener<Arg>;

    using Connection = TcpConnection<Arg>;
    
    inline static constexpr TcpSeqInt MaxRcvWnd = Constants::MaxWindow;
    
    inline PlatformFacade<typename Arg::PlatformImpl> platform () const
    {
        return proto().platform();
    }
};

}

#endif
