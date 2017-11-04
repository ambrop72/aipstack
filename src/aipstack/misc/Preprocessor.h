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

#ifndef AIPSTACK_PREPROCESSOR_H
#define AIPSTACK_PREPROCESSOR_H

/**
 * @ingroup misc
 * @defgroup preprocessor Preprocessor-based Utilities
 * @brief Macros to concisely bring entities in a different scope into the current one.
 * 
 * @{
 */

/**
 * Expands to a type alias bringing a type from a different scope into this scope.
 * 
 * This version uses typename.
 * 
 * @param namespace Scope which contains the type.
 * @param type_name Type in that scope to create an alias for.
 */
#define AIPSTACK_USE_TYPE1(namespace, type_name) using type_name = typename namespace::type_name;

/**
 * Expands to a type alias bringing a type from a different scope into this scope.
 * 
 * This version does not use typename.
 * 
 * @param namespace Scope which contains the type.
 * @param type_name Type in that scope to create an alias for.
 */
#define AIPSTACK_USE_TYPE2(namespace, type_name) using type_name = namespace::type_name;

/**
 * Expands to a variable definition which copies a value from another scope into this scope.
 * 
 * Note that this also works for non-template functions. If using this at class level, a
 * corresponding out-of-class definition (without an initializer) may be needed depending
 * on the type and usage.
 * 
 * @param namespace Scope which contains the value.
 * @param value_name Name of variable in that scope to initialize this variable to.
 */
#define AIPSTACK_USE_VAL(namespace, value_name) static constexpr auto value_name = namespace::value_name;

#ifdef IN_DOXYGEN

/**
 * Expands to type aliases bringing types from a different scope into this scope.
 * 
 * This expands to @ref AIPSTACK_USE_TYPE1 for each specified type name.
 * 
 * @param namespace Scope which contains the types.
 * @param type_names Names of types in that scope to create an alias for, given as a
 *        parenthesized list separated by commas, e.g. `(T1, T2, T3)`.
 */
#define AIPSTACK_USE_TYPES1(namespace, type_names) implementation_hidden

/**
 * Expands to type aliases bringing types from a different scope into this scope.
 * 
 * This expands to @ref AIPSTACK_USE_TYPE2 for each specified type name.
 * 
 * @param namespace Scope which contains the types.
 * @param type_names Names of types in that scope to create an alias for, given as a
 *        parenthesized list separated by commas, e.g. `(T1, T2, T3)`.
 */
#define AIPSTACK_USE_TYPES2(namespace, type_names) implementation_hidden

/**
 * Expands to variable definitions which copy values from another scope into this scope.
 * 
 * This expands to @ref AIPSTACK_USE_VAL for each specified variable name.
 * 
 * @param namespace Scope which contains the variables.
 * @param value_names Names of variables in that scope to initialize these variables to,
 *        given as a parenthesized list separated by commas, e.g. `(V1, V2, V3)`.
 */
#define AIPSTACK_USE_VALS(namespace, value_names) implementation_hidden

#else

#define AIPSTACK_JOIN_HELPER(x, y) x##y
#define AIPSTACK_JOIN(x, y) AIPSTACK_JOIN_HELPER(x, y)

#define AIPSTACK_REMOVE_PARENS(...) __VA_ARGS__

#define AIPSTACK_EXPAND(x) x

#include "Preprocessor_MacroMap.h"

#define AIPSTACK_USE_TYPES1(namespace, type_names) AIPSTACK_AS_MAP(AIPSTACK_USE_TYPE1, AIPSTACK_AS_MAP_DELIMITER_NONE, namespace, type_names)
#define AIPSTACK_USE_TYPES2(namespace, type_names) AIPSTACK_AS_MAP(AIPSTACK_USE_TYPE2, AIPSTACK_AS_MAP_DELIMITER_NONE, namespace, type_names)
#define AIPSTACK_USE_VALS(namespace, value_names)  AIPSTACK_AS_MAP(AIPSTACK_USE_VAL,   AIPSTACK_AS_MAP_DELIMITER_NONE, namespace, value_names)

#endif

/** @} */

#endif
