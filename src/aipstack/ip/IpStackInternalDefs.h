/*
 * Copyright (c) 2020 Ambroz Bizjak
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

#ifndef AIPSTACK_IPSTACK_INTERNAL_DEFS_H
#define AIPSTACK_IPSTACK_INTERNAL_DEFS_H

#include <aipstack/structure/LinkModel.h>

namespace AIpStack {

#ifndef IN_DOXYGEN

template<typename> class IpStack;
template<typename> class IpIface;
template<typename> class IpIfaceListener;

// This class provides some types that cannot be defined in IpStack because that
// would cause circular dependency problems, e.g. from IpIface.
template<typename Arg>
class IpStackInternalDefs
{
    template<typename> friend class IpStack;
    template<typename> friend class IpIface;
    template<typename> friend class IpIfaceListener;

private:
    using IfaceLinkModel = PointerLinkModel<IpIface<Arg>>;
    using IfaceListenerLinkModel = PointerLinkModel<IpIfaceListener<Arg>>;
};

#endif

}

#endif
