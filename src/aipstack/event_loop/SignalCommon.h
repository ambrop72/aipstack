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

/**
 * Types of operating system signals, possibly used as a bitmask.
 * 
 * The set of signals in this enum does not depend on the platform but actual support for
 * specific signals in facilities such as @ref SignalCollector and @ref
 * nativeNameForSignalType does.
 * 
 * Operators provided by @ref AIPSTACK_ENUM_BITFIELD_OPS are available.
 */
enum class SignalType {
    None         = 0, /**< Zero value represening no signals. */
    Interrupt    = 1 << 0, /**< Interrupt signal (SIGINT in \*nix). */
    Terminate    = 1 << 1, /**< Terminate signal (SIGTERM in \*nix). */
    Hangup       = 1 << 2, /**< Hangup signal (SIGHUP in \*nix). */
    Quit         = 1 << 3, /**< Quit signal (SIGQUIT in \*nix). */
    User1        = 1 << 4, /**< User-defined signal 1 (SIGUSR1 in \*nix). */
    User2        = 1 << 5, /**< User-defined signal 2 (SIGUSR2 in \*nix). */
    Child        = 1 << 6, /**< Child stopped or terminated signal (SIGCHLD in \*nix). */
    Alarm        = 1 << 7, /**< Alarm signal (SIGALRM in \*nix). */
    InputOutput  = 1 << 8, /**< Input/output possible signal (SIGIO in \*nix). */
    WindowResize = 1 << 9, /**< Window resize signal (SIGWINCH in \*nix). */
    Break        = 1 << 10, /**< Break signal (Window-only). */
    /**
     * Mask of signals commonly understood as request for the program to exit.
     * Currently this includes Interrupt, Terminate, Hangup, Quit and Break.
     */
    ExitSignals  = Interrupt|Terminate|Hangup|Quit|Break,
};
#ifndef IN_DOXYGEN
AIPSTACK_ENUM_BITFIELD_OPS(SignalType)
#endif

/**
 * Get the platform-native name for a signal represented by a @ref SignalType enum value.
 * 
 * On \*nix, this returns the upper-case signal names such as "SIGINT".
 * 
 * On Windows it returns:
 * - "CTRL_C_EVENT" for @ref SignalType::Interrupt,
 * - "CTRL_BREAK_EVENT" for @ref SignalType::Break,
 * - "CTRL_CLOSE_EVENT" for @ref SignalType::Hangup.
 * 
 * If the signal is not one of the signals defined by the @ref SignalType enum or is not
 * supported for the platform, this function returns "unknown".
 * 
 * @param signal Signal to get the name of (one signal not mask). This functions tolerates
 *        any value of this argument.
 * @return Name of the signal as a null-terminated string (or "unknown", see above). The
 *         string is in static storage.
 */
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
