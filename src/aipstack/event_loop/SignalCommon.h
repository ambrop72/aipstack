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

#ifndef AIPSTACK_SIGNAL_COMMON_H
#define AIPSTACK_SIGNAL_COMMON_H

#if defined(__linux__)
#include <signal.h>
#endif

#include <aipstack/misc/EnumBitfieldUtils.h>

namespace AIpStack {

/**
 * @addtogroup event-loop
 * @{
 */

enum class SignalType {
    None         = 0,
    Interrupt    = 1 << 0,
    Terminate    = 1 << 1,
    Hangup       = 1 << 2,
    Quit         = 1 << 3,
    User1        = 1 << 4,
    User2        = 1 << 5,
    Child        = 1 << 6,
    Alarm        = 1 << 7,
    InputOutput  = 1 << 8,
    WindowResize = 1 << 9,
    Break        = 1 << 10,
    ExitSignals  = Interrupt|Terminate|Hangup|Quit|Break,
};
#ifndef IN_DOXYGEN
AIPSTACK_ENUM_BITFIELD_OPS(SignalType)
#endif

char const * nativeNameForSignalType(SignalType signal);

#ifndef IN_DOXYGEN

#if defined(__linux__)

int signalTypeToSignum(SignalType signal);

SignalType signumToSignalType(int signum);

void addSignalsToSet(SignalType signals, ::sigset_t &set);

void initSigSetToSignals(::sigset_t &set, SignalType signals);

SignalType getSignalsFromSigSet(::sigset_t const &set);

#endif

#endif

/** @} */

}

#endif
