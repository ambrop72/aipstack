/*
 * Copyright (c) 2013 Ambroz Bizjak
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

#ifndef AIPSTACK_ASSERT_H
#define AIPSTACK_ASSERT_H

#include <aipstack/misc/Hints.h>

/**
 * @ingroup misc
 * @defgroup assertions Assertion Utilities
 * @brief Assertions and fatal error handling.
 * 
 * The utilities in this module are used throughout the stack for assertions. The most
 * commonly used thing is the @ref AIPSTACK_ASSERT macro, which is a claim that that some
 * expression should by all logic evaluate to true.
 * 
 * Assertions are disabled by default and can be enabled by the application by defining
 * the macro `AIPSTACK_CONFIG_ENABLE_ASSERTIONS` (to anything). If assertions are enabled,
 * then the expressions in @ref AIPSTACK_ASSERT are evaluated and checked, otherwise they
 * are not evaluated. The macro @ref AIPSTACK_ASSERTIONS can be used to check if assertions
 * are enabled; this is useful to avoid executing code which is only needed by assertions
 * but is not itself an assertion.
 * 
 * The @ref AIPSTACK_ASSERT_FORCE and @ref AIPSTACK_ASSERT_FORCE_MSG macros provide
 * assertions which are always evaluated and checked.
 * 
 * If an assertion evaluates to false, the so-called assert-abort handler is invoked whose
 * job is to somehow record the error and terminate the program or at least never return.
 * The default assert-abort handler will print an error message to stderr and call
 * `abort()`. The assert-abort handler can be directly invoked using the macro
 * @ref AIPSTACK_ASSERT_ABORT.
 * 
 * The application can specify the assert-abort handler by defining the macro
 * `AIPSTACK_CONFIG_ASSERT_HANDLER`. This macro would be used as the name of a function to
 * call with a single parameter, the error message as `char const *`. In order to ensure
 * that any declarations needed by the defined assert-abort handler are available, the
 * application can specify an include file using the macro `AIPSTACK_CONFIG_ASSERT_INCLUDE`;
 * the file would be included within Assert.h as follows:
 * 
 * ```
 * #ifdef AIPSTACK_CONFIG_ASSERT_INCLUDE
 * #include AIPSTACK_CONFIG_ASSERT_INCLUDE
 * #endif
 * ```
 * 
 * @{
 */

#ifdef AIPSTACK_CONFIG_ASSERT_INCLUDE
#include AIPSTACK_CONFIG_ASSERT_INCLUDE
#endif

#ifndef IN_DOXYGEN

#ifdef AIPSTACK_CONFIG_ASSERT_HANDLER
#define AIPSTACK_HAS_EXTERNAL_ASSERT_HANDLER 1
#define AIPSTACK_ASSERT_HANDLER AIPSTACK_CONFIG_ASSERT_HANDLER
#else
#define AIPSTACK_HAS_EXTERNAL_ASSERT_HANDLER 0
#define AIPSTACK_ASSERT_HANDLER(msg) \
    AIpStack_AssertAbort(__FILE__, __LINE__, msg)
#endif

#endif

#ifdef IN_DOXYGEN

/**
 * Indicates whether assertions are enabled.
 * 
 * Assertions are enabled if and only if the macro `AIPSTACK_CONFIG_ENABLE_ASSERTIONS` is
 * defined by the application (see the @ref assertions module description). Applications
 * must not define (or undefine) *this* macro (`AIPSTACK_ASSERTIONS`).
 */
#define AIPSTACK_ASSERTIONS implementation hidden (1 or 0)

/**
 * Assert that some expression is true.
 * 
 * If assertions are enabled (see the @ref assertions module description) then this is
 * equivalent to @ref AIPSTACK_ASSERT_FORCE, otherwise it does nothing (and does not
 * evaluate the expression).
 * 
 * This macro expands to a block construct which generally does not require a semicolon.
 * 
 * @param e Expression to assert. It is evaluated if and only if assertions are
 *          enabled.
 */
#define AIPSTACK_ASSERT(e) { implementation hidden }

#else

#ifdef AIPSTACK_CONFIG_ENABLE_ASSERTIONS
#define AIPSTACK_ASSERTIONS 1
#define AIPSTACK_ASSERT(e) AIPSTACK_ASSERT_FORCE(e)
#else
#define AIPSTACK_ASSERTIONS 0
#define AIPSTACK_ASSERT(e) {}
#endif

#endif

/**
 * Call the assert-abort handler with the given error message.
 * 
 * This calls the assert-abort handler which is supposed to recort the error and never
 * return. This may be either the default or an application-provided handler (see the
 * @ref assertions module description).
 * 
 * This macro expands to a do-while-0 construct which requires a semicolon.
 * 
 * @param msg Error message as `char const *`.
 */
#define AIPSTACK_ASSERT_ABORT(msg) do { AIPSTACK_ASSERT_HANDLER(msg); } while (0)

/**
 * Evaluate an expression and invoke the assert-abort handler if the result is false.
 * 
 * This macro expands to a block construct which generally does not require a semicolon.
 * 
 * @param e Expression to evaluate, used as the condition of an `if` clause.
 */
#define AIPSTACK_ASSERT_FORCE(e) { if (e) {} else AIPSTACK_ASSERT_ABORT(#e); }

/**
 * Evaluate an expression and invoke the assert-abort handler if the result is false.
 * 
 * This is like @ref AIPSTACK_ASSERT_FORCE but with a custom error message.
 * 
 * This macro expands to a block construct which generally does not require a semicolon.
 * 
 * @param e Expression to evaluate, used as the condition of an `if` clause.
 * @param msg Error message as `char const *`.
 */
#define AIPSTACK_ASSERT_FORCE_MSG(e, msg) { if (e) {} else AIPSTACK_ASSERT_ABORT(msg); }

#ifndef IN_DOXYGEN
#if !AIPSTACK_HAS_EXTERNAL_ASSERT_HANDLER

#ifdef __cplusplus
#include <cstdio>
#include <cstdlib>
#define AIPSTACK_ASSERT_STD std::
#else
#include <stdio.h>
#include <stdlib.h>
#define AIPSTACK_ASSERT_STD
#endif

#ifdef __cplusplus
extern "C"
#endif
AIPSTACK_NO_INLINE AIPSTACK_NO_RETURN
inline void AIpStack_AssertAbort (char const *file, unsigned int line, char const *msg)
{
    AIPSTACK_ASSERT_STD fprintf(stderr,
        "AIpStack %s:%u: Assertion `%s' failed.\n", file, line, msg);
    AIPSTACK_ASSERT_STD abort();
}

#undef AIPSTACK_ASSERT_STD

#endif
#endif

/** @} */

#endif
