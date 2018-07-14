/*
 * Copyright (c) 2018 Ambroz Bizjak
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

#ifndef AIPSTACK_TCP_PCB_KEY_H
#define AIPSTACK_TCP_PCB_KEY_H

#include <aipstack/ip/IpAddr.h>

namespace AIpStack {

// Lookup key for TCP PCBs
struct TcpPcbKey : public Ip4AddrPair {
    TcpPcbKey () = default;
    
    inline TcpPcbKey (
        Ip4Addr local_addr_, Ip4Addr remote_addr_,
        PortNum local_port_, PortNum remote_port_)
    :
        Ip4AddrPair{local_addr_, remote_addr_},
        local_port(local_port_),
        remote_port(remote_port_)
    {}
    
    PortNum local_port;
    PortNum remote_port;
};

// Provides comparison functions for TcpPcbKey
class TcpPcbKeyCompare {
public:
    static int CompareKeys (TcpPcbKey const &op1, TcpPcbKey const &op2)
    {
        // Compare in an order that would need least
        // comparisons with typical server usage.
        
        if (op1.remote_port < op2.remote_port) {
            return -1;
        }
        if (op1.remote_port > op2.remote_port) {
            return 1;
        }
        
        if (op1.remote_addr < op2.remote_addr) {
            return -1;
        }
        if (op1.remote_addr > op2.remote_addr) {
            return 1;
        }
        
        if (op1.local_port < op2.local_port) {
            return -1;
        }
        if (op1.local_port > op2.local_port) {
            return 1;
        }

        if (op1.local_addr < op2.local_addr) {
            return -1;
        }
        if (op1.local_addr > op2.local_addr) {
            return 1;
        }
        
        return 0;
    }
    
    static bool KeysAreEqual (TcpPcbKey const &op1, TcpPcbKey const &op2)
    {
        return op1.remote_port == op2.remote_port &&
               op1.remote_addr == op2.remote_addr &&
               op1.local_port  == op2.local_port  &&
               op1.local_addr  == op2.local_addr;
    }
};

}

#endif
