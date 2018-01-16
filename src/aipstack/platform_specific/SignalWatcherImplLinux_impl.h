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
#include <utility>

#include <unistd.h>
#include <signal.h>
#include <sys/signalfd.h>

#include <aipstack/misc/Assert.h>
#include <aipstack/misc/MinMax.h>
#include <aipstack/platform_specific/SignalWatcherImplLinux.h>

namespace AIpStack {

SignalWatcherImplLinux::SignalWatcherImplLinux (EventLoop &loop, SignalBlocker &blocker) :
    m_blocker(blocker),
    m_watcher(loop, AIPSTACK_BIND_MEMBER(&SignalWatcherImplLinux::watcherHandler, this))
{}

void SignalWatcherImplLinux::start (SignalType signals)
{
    AIPSTACK_ASSERT(!m_signalfd_fd)

    if ((signals & ~m_blocker.getBlockedSignals()) != EnumZero) {
        throw std::logic_error("Not all watched signals are blocked in SignaBlocker.");
    }

    ::sigset_t sset;
    initSigSetToSignals(sset, signals);

    FileDescriptorWrapper fd(::signalfd(-1, &sset, SFD_NONBLOCK|SFD_CLOEXEC));
    if (!fd) {
        throw std::runtime_error("signalfd() failed for SignalWatcherImplLinux");
    }

    m_watcher.initFd(*fd, EventLoopFdEvents::Read);

    // If we did anything else here that could fail we would need to reset m_watcher!

    m_signalfd_fd = std::move(fd);
}

void SignalWatcherImplLinux::stop ()
{
    AIPSTACK_ASSERT(m_signalfd_fd)

    // First reset watcher then close fd.
    m_watcher.reset();
    m_signalfd_fd = FileDescriptorWrapper();
}

void SignalWatcherImplLinux::watcherHandler(EventLoopFdEvents events)
{
    AIPSTACK_ASSERT(m_signalfd_fd)
    (void)events;

    struct signalfd_siginfo siginfo;
    auto bytes = ::read(*m_signalfd_fd, &siginfo, sizeof(siginfo));

    if (bytes < 0) {
        int error = errno;
        if (FileDescriptorWrapper::errIsEAGAINorEWOULDBLOCK(error)) {
            return;
        }

        std::fprintf(stderr, "SignalWatcher: read from signalfd failed, err=%d\n", error);
        return;
    }

    AIPSTACK_ASSERT(bytes == sizeof(siginfo))

    if (siginfo.ssi_signo > TypeMax<int>()) {
        std::fprintf(stderr,
            "SignalWatcher: read signal number is out of range for int.\n");
        return;
    }
    int signum = siginfo.ssi_signo;

    SignalType sig = signumToSignalType(signum);
    if (sig == SignalType::None) {
        std::fprintf(stderr, "SignalWatcher: read signal number is not requested.\n");
        return;
    }

    return SignalWatcherImplBase::callHandler(SignalInfo{sig});
}

}
