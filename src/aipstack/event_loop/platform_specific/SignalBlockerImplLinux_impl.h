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

#include <stdio.h>
#include <stdexcept>

#include <signal.h>

#include <aipstack/event_loop/platform_specific/SignalBlockerImplLinux.h>

namespace AIpStack {

void SignalBlockerImplLinux::block (SignalType signals)
{
    ::sigset_t sset;
    initSigSetToSignals(sset, signals);

    ::sigset_t orig_sset;
    if (::pthread_sigmask(SIG_BLOCK, &sset, &orig_sset) != 0) {
        throw std::runtime_error("pthread_sigmask failed for SignalBlocker.");
    }

    m_orig_blocked_signals = getSignalsFromSigSet(orig_sset);
}

void SignalBlockerImplLinux::unblock (SignalType blocked_signals)
{
    SignalType unblock_signals = ~m_orig_blocked_signals & blocked_signals;

    ::sigset_t sset;
    initSigSetToSignals(sset, unblock_signals);

    if (::pthread_sigmask(SIG_UNBLOCK, &sset, nullptr) != 0) {
        std::fprintf(stderr, "pthread_sigmask failed to unblock signals.");
    }
}

}
