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

#include <cerrno>
#include <cstdio>
#include <stdexcept>

#include <unistd.h>
#include <signal.h>
#include <sys/signalfd.h>

#include <aipstack/misc/Assert.h>
#include <aipstack/misc/MinMax.h>
#include <aipstack/misc/Function.h>
#include <aipstack/event_loop/FormatString.h>
#include <aipstack/event_loop/SignalCommon.h>
#include <aipstack/event_loop/platform_specific/SignalWatcherImplLinux.h>

namespace AIpStack {

SignalCollectorImplLinux::SignalCollectorImplLinux ()
{
    SignalType signals = SignalCollectorImplBase::baseGetSignals();

    ::sigset_t sset;
    initSigSetToSignals(sset, signals);

    ::sigset_t orig_sset;
    if (::pthread_sigmask(SIG_BLOCK, &sset, &orig_sset) != 0) {
        throw std::runtime_error(formatString(
            "SignalCollector: pthread_sigmask failed to block signals, err=%d", errno));
    }

    m_orig_blocked_signals = getSignalsFromSigSet(orig_sset);
}

SignalCollectorImplLinux::~SignalCollectorImplLinux ()
{
    SignalType signals = SignalCollectorImplBase::baseGetSignals();
    SignalType unblock_signals = signals & ~m_orig_blocked_signals;

    ::sigset_t sset;
    initSigSetToSignals(sset, unblock_signals);

    if (::pthread_sigmask(SIG_UNBLOCK, &sset, nullptr) != 0) {
        std::fprintf(stderr,
            "SignalCollector: pthread_sigmask failed to unblock signals, err=%d\n", errno);
    }
}

SignalWatcherImplLinux::SignalWatcherImplLinux () :
    m_fd_watcher(SignalWatcherImplBase::getEventLoop(),
                 AIPSTACK_BIND_MEMBER(&SignalWatcherImplLinux::fdWatcherHandler, this))
{
    SignalType signals = getCollector().SignalCollectorImplBase::baseGetSignals();

    ::sigset_t sset;
    initSigSetToSignals(sset, signals);

    m_signalfd_fd = FileDescriptorWrapper(::signalfd(-1, &sset, SFD_NONBLOCK|SFD_CLOEXEC));
    if (!m_signalfd_fd) {
        throw std::runtime_error(formatString(
            "SignalWatcher: signalfd failed to create signalfd, err=%d", errno));
    }

    m_fd_watcher.initFd(*m_signalfd_fd, EventLoopFdEvents::Read);
}

SignalWatcherImplLinux::~SignalWatcherImplLinux ()
{}

SignalCollectorImplLinux & SignalWatcherImplLinux::getCollector () const
{
    return static_cast<SignalCollectorImplLinux &>(SignalWatcherImplBase::getCollector());
}

void SignalWatcherImplLinux::fdWatcherHandler(EventLoopFdEvents events)
{
    (void)events;

    struct signalfd_siginfo siginfo;
    auto bytes = ::read(*m_signalfd_fd, &siginfo, sizeof(siginfo));

    if (bytes < 0) {
        int err = errno;
        if (FileDescriptorWrapper::errIsEAGAINorEWOULDBLOCK(err)) {
            return;
        }

        std::fprintf(stderr, "SignalWatcher: read from signalfd failed, err=%d\n", err);
        return;
    }

    AIPSTACK_ASSERT(bytes == sizeof(siginfo));

    if (siginfo.ssi_signo > TypeMax<int>) {
        std::fprintf(stderr,
            "SignalWatcher: read signal number is out of range for int.\n");
        return;
    }
    int signum = int(siginfo.ssi_signo);

    SignalType sig = signumToSignalType(signum);
    if (sig == SignalType::None) {
        std::fprintf(stderr, "SignalWatcher: read signal number not recognized.\n");
        return;
    }

    SignalType signals = getCollector().SignalCollectorImplBase::baseGetSignals();
    if ((sig & signals) == Enum0) {
        std::fprintf(stderr, "SignalWatcher: read signal number is not requested.\n");
        return;
    }

    return SignalWatcherImplBase::callHandler(SignalInfo{sig});
}

}
