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

#include <aipstack/event_loop/SignalCommon.h>

namespace AIpStack {

#if defined(__linux__)

#define AIPSTACK_FOR_ALL_SIGNALS(X) \
    X(SignalType::Interrupt,    SIGINT) \
    X(SignalType::Terminate,    SIGTERM) \
    X(SignalType::Hangup,       SIGHUP) \
    X(SignalType::Quit,         SIGQUIT) \
    X(SignalType::User1,        SIGUSR1) \
    X(SignalType::User2,        SIGUSR2) \
    X(SignalType::Child,        SIGCHLD) \
    X(SignalType::Alarm,        SIGALRM) \
    X(SignalType::InputOutput,  SIGIO) \
    X(SignalType::WindowResize, SIGWINCH)

char const * nativeNameForSignalType(SignalType signal)
{
    #define AIPSTACK_NATIVE_NAME_FOR_SIGNAL_TYPE_CASE(sig, signum) \
        case sig: return #signum;
    
    switch (signal) {
        AIPSTACK_FOR_ALL_SIGNALS(AIPSTACK_NATIVE_NAME_FOR_SIGNAL_TYPE_CASE)
        default: return "unknown";
    }

    #undef AIPSTACK_NATIVE_NAME_FOR_SIGNAL_TYPE_CASE
}

int signalTypeToSignum(SignalType signal)
{
    #define AIPSTACK_SIGNAL_TYPE_TO_SIGNUM_CASE(sig, signum) \
        case sig: return signum;
    
    switch (signal) {
        AIPSTACK_FOR_ALL_SIGNALS(AIPSTACK_SIGNAL_TYPE_TO_SIGNUM_CASE)
        default: return -1;
    }

    #undef AIPSTACK_SIGNAL_TYPE_TO_SIGNUM_CASE
}

SignalType signumToSignalType(int signum)
{
    #define AIPSTACK_SIGNUM_TO_SIGNAL_TYPE_CASE(sig, signum) \
        case signum: return sig;

    switch (signum) {
        AIPSTACK_FOR_ALL_SIGNALS(AIPSTACK_SIGNUM_TO_SIGNAL_TYPE_CASE)
        default: return SignalType::None;
    }

    #undef AIPSTACK_SIGNUM_TO_SIGNAL_TYPE_CASE
}

void addSignalsToSet(SignalType signals, ::sigset_t &set)
{
    auto add_if_present = [&](SignalType sig, int signum) {
        if ((signals & sig) != EnumZero) {
            ::sigaddset(&set, signum);
        }
    };

    #define AIPSTACK_ADD_SIGNALS_TO_SET_SIGNAL(sig, signum) \
    add_if_present(sig, signum);

    AIPSTACK_FOR_ALL_SIGNALS(AIPSTACK_ADD_SIGNALS_TO_SET_SIGNAL)

    #undef AIPSTACK_ADD_SIGNALS_TO_SET_SIGNAL
}

void initSigSetToSignals(::sigset_t &set, SignalType signals)
{
    ::sigemptyset(&set);
    addSignalsToSet(signals, set);
}

SignalType getSignalsFromSigSet(::sigset_t const &set)
{
    SignalType signals = SignalType::None;

    #define AIPSTACK_GET_SIGNALS_FROM_SIGSET_SIGNAL(sig, signum) \
    if (::sigismember(&set, signum)) { \
        signals |= sig; \
    }

    AIPSTACK_FOR_ALL_SIGNALS(AIPSTACK_GET_SIGNALS_FROM_SIGSET_SIGNAL)

    #undef AIPSTACK_GET_SIGNALS_FROM_SIGSET_SIGNAL

    return signals;
}

#endif

}
