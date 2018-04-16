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

#ifndef AIPSTACK_PRAGMA_H
#define AIPSTACK_PRAGMA_H

/**
 * @addtogroup misc
 * @{
 */

#if defined(__GNUC__) || defined(__clang__) || defined(IN_DOXYGEN)

/**
 * Adjusts diagnostic behavior in a code section with GCC-compatible compilers.
 * 
 * Each use of this macro must be matched with a subsequent use of the @ref
 * AIPSTACK_GCC_DIAGNOSTIC_END macro. Recursive use is possible.
 * 
 * For example, to disable the warning `-Wunused-value` in a section of code:
 * ```
 * AIPSTACK_GCC_DIAGNOSTIC_START("GCC diagnostic ignored \"-Wunused-value\"")
 * // code where the warning is disabled
 * AIPSTACK_GCC_DIAGNOSTIC_END
 * ```
 * 
 * @note It is very important to not forget the end macro, as that would result in
 * the changes being effective in the rest of the translation unit including
 * subsequently included headers.
 * 
 * This macro expands to nothing if GCC diagnostic pragmas are not considered to be
 * supported.
 * 
 * @param pragma_str The string for the `_Pragma` directive which will modify diagnostic
 *        behavior as desired.
 */
#define AIPSTACK_GCC_DIAGNOSTIC_START(pragma_str) \
    _Pragma("GCC diagnostic push") _Pragma(pragma_str)

/**
 * Restore diagnostic behavior (for use with @ref AIPSTACK_GCC_DIAGNOSTIC_START).
 * 
 * This macro expands to nothing if GCC diagnostic pragmas are not considered to be
 * supported.
 */
#define AIPSTACK_GCC_DIAGNOSTIC_END \
    _Pragma("GCC diagnostic pop")

#else

#define AIPSTACK_GCC_DIAGNOSTIC_START(pragma_str)
#define AIPSTACK_GCC_DIAGNOSTIC_END

#endif

/** @} */

#endif
