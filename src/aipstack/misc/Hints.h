/*
 * Copyright (c) 2016 Ambroz Bizjak
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

#ifndef AIPSTACK_HINTS_H
#define AIPSTACK_HINTS_H

/**
 * @ingroup misc
 * @defgroup hints Optimization Hints
 * @brief Optimization hints for compilers.
 * 
 * The macros in this module may involve compiler-specific optimization hints, but no effect
 * is guaranteed. The definitions given in this documentation are what these macros are
 * functionally equivalent to.
 * 
 * The macros @ref AIPSTACK_LIKELY and @ref AIPSTACK_UNLIKELY provide hints to the compiler
 * to optimize for the case where the expression evaluates to true or false respectively.
 * They are most commonly used within `if` clauses, for example:
 * 
 * ```
 * if (AIPSTACK_LIKELY(x == 5)) {
 *     // ...
 * }
 * if (AIPSTACK_UNLIKELY(ptr == nullptr) {
 *     // ...
 * }
 * ```
 * 
 * The macros @ref AIPSTACK_ALWAYS_INLINE, @ref AIPSTACK_NO_INLINE, @ref AIPSTACK_NO_RETURN,
 * @ref AIPSTACK_UNROLL_LOOPS and @ref AIPSTACK_OPTIMIZE_SIZE can be understood as function
 * attributes and are used in the list of qualifiers before the return type. For a template
 * function, that would be after the template parameters. See the examples below.
 * 
 * ```
 * AIPSTACK_ALWAYS_INLINE
 * int function1 (int arg) {
 *     return arg + 1;
 * }
 * 
 * template <typename T>
 * AIPSTACK_NO_INLINE
 * void function2 (T arg) {
 *     arg.foo();
 * }
 * ```
 * @{
 */

#if defined(IN_DOXYGEN) || !defined(__GNUC__)

/**
 * Convert to bool and optimize for the true case.
 * 
 * @param x Input value.
 * @return The value converted to boolean.
 */
#define AIPSTACK_LIKELY(x) (!!(x))

/**
 * Convert to bool and optimize for the false case.
 * 
 * @param x Input value.
 * @return The value converted to boolean.
 */
#define AIPSTACK_UNLIKELY(x) (!!(x))

/**
 * Make a function inline and prefer to actually inline it.
 * 
 * This is used in the list of qualifiers before the return type. The `inline` qualifier is
 * included and should not be specified directly when using this macro.
 */
#define AIPSTACK_ALWAYS_INLINE inline

/**
 * Prefer not to inline a function.
 * 
 * This is used in the list of qualifiers before the return type.
 */
#define AIPSTACK_NO_INLINE

/**
 * Indicate that the function never returns.
 * 
 * This is used in the list of qualifiers before the return type. This may only be used when
 * the function does in fact never return.
 */
#define AIPSTACK_NO_RETURN

/**
 * Prefer to unroll loops in a function.
 * 
 * This is used in the list of qualifiers before the return type.
 */
#define AIPSTACK_UNROLL_LOOPS

/**
 * Prefer to optimize the function for code size.
 * 
 * This is used in the list of qualifiers before the return type.
 */
#define AIPSTACK_OPTIMIZE_SIZE

#else

#define AIPSTACK_LIKELY(x) __builtin_expect(!!(x), 1)
#define AIPSTACK_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define AIPSTACK_ALWAYS_INLINE __attribute__((always_inline)) inline
#define AIPSTACK_NO_INLINE __attribute__((noinline))
#define AIPSTACK_NO_RETURN __attribute__((noreturn))

#ifndef __clang__
#define AIPSTACK_UNROLL_LOOPS __attribute__((optimize("unroll-loops")))
#define AIPSTACK_OPTIMIZE_SIZE __attribute__((optimize("Os")))
#else
#define AIPSTACK_UNROLL_LOOPS
#define AIPSTACK_OPTIMIZE_SIZE
#endif

#endif

/** @} */

#endif
