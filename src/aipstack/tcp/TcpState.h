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

#ifndef AIPSTACK_TCP_STATE_H
#define AIPSTACK_TCP_STATE_H

#include <cstdint>

#include <aipstack/misc/OneOf.h>

namespace AIpStack {

/**
 * TCP states.
 * 
 * ATTENTION: The bit values are carefully crafted to allow
 * efficient implementation of the following state predicates:
 * - isSynSentOrRcvd,
 * - isAcceptingData,
 * - canOutput,
 * - isSndOpen.
 * 
 * NOTE: the FIN_WAIT_2_TIME_WAIT state is not a standard TCP
 * state but is used transiently when we were in FIN_WAIT_2 and
 * have just received a FIN, but we will only go to TIME_WAIT
 * after calling callbacks.
 */
class TcpState {
public:
    using ValueType = std::uint8_t;

private:
    ValueType m_value;

public:
    inline static constexpr int Bits = 4;

    TcpState () = delete;

    inline constexpr explicit TcpState (ValueType value) :
        m_value(value)
    {}

    inline constexpr ValueType value () const {
        return m_value;
    }

    inline constexpr bool operator== (TcpState other) const {
        return value() == other.value();
    }

    inline constexpr bool operator!= (TcpState other) const {
        return value() != other.value();
    }

    inline constexpr bool isActive () const;

    inline constexpr bool isSynSentOrRcvd () const;

    inline constexpr bool isAcceptingData () const;

    inline constexpr bool canOutput () const;

    inline constexpr bool isSndOpen () const;
};

namespace TcpStates {
    inline constexpr TcpState CLOSED               = TcpState(0b0101);
    inline constexpr TcpState SYN_SENT             = TcpState(0b1101);
    inline constexpr TcpState SYN_RCVD             = TcpState(0b1100);
    inline constexpr TcpState ESTABLISHED          = TcpState(0b0000);
    inline constexpr TcpState CLOSE_WAIT           = TcpState(0b0001);
    inline constexpr TcpState LAST_ACK             = TcpState(0b1000);
    inline constexpr TcpState FIN_WAIT_1           = TcpState(0b0010);
    inline constexpr TcpState FIN_WAIT_2           = TcpState(0b0100);
    inline constexpr TcpState FIN_WAIT_2_TIME_WAIT = TcpState(0b1111);
    inline constexpr TcpState CLOSING              = TcpState(0b1011);
    inline constexpr TcpState TIME_WAIT            = TcpState(0b1110);    
}

constexpr bool TcpState::isActive () const {
    return *this != OneOf(TcpStates::CLOSED, TcpStates::SYN_SENT,
                          TcpStates::SYN_RCVD, TcpStates::TIME_WAIT);
}

constexpr bool TcpState::isSynSentOrRcvd () const {
    //return *this == OneOf(TcpStates::SYN_SENT, TcpStates::SYN_RCVD);
    return (value() >> 1) == (0b1100 >> 1);
}

constexpr bool TcpState::isAcceptingData () const {
    //return *this == OneOf(TcpStates::ESTABLISHED,
    //                      TcpStates::FIN_WAIT_1, TcpStates::FIN_WAIT_2);
    return (value() & 0b1001) == 0;
}

constexpr bool TcpState::canOutput () const {
    //return *this == OneOf(TcpStates::ESTABLISHED, TcpStates::FIN_WAIT_1,
    //                  TcpStates::CLOSING, TcpStates::CLOSE_WAIT, TcpStates::LAST_ACK);
    return (value() & 0b0100) == 0;
}

constexpr bool TcpState::isSndOpen () const {
    //return *this == OneOf(TcpStates::ESTABLISHED, TcpStates::CLOSE_WAIT);
    return (value() >> 1) == 0;
}

}

#endif
