/*
 * Copyright (c) 2017 Ambroz Bizjak
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

#ifndef AIPSTACK_INSTANCE_H
#define AIPSTACK_INSTANCE_H

#include <aipstack/misc/Preprocessor.h>

/**
 * @ingroup infra
 * @defgroup instance Template Class Instantiation
 * @brief Mechanism to instantiate class templates with an identity.
 * 
 * The purpose of this mechanism is to prevent the explosion of template type expressions in
 * symbol names, especially within debug symbols. For example, consider a class which is
 * configured using the @ref configuration module:
 * 
 * ```
 * template <typename ...Options>
 * class MyClass {
 *     AIPSTACK_OPTION_CONFIG_VALUE(MyClassOptions, Option1)
 * public:
 *     class NestedClass {};
 *     void function(NestedClass n);
 * };
 * ```
 * 
 * If the class template is instantiated as `MyClass<MyClassOptions::Option1::Is<5>>`, then
 * the full signature of `function` is:
 * 
 * ```
 * void MyClass<MyClassOptions::Option1::Is<5>>::function(MyClass<MyClassOptions::Option1::Is<5>>::NestedClass)
 * ```
 * 
 * As can be seen, the entire configuration of the class is repeated twice in the signature
 * of `function`. In a real application where modules have many options and are instantiated
 * hierarchially, such native use of the configuration system can easily result in an
 * explosion of signature lengths, causing extreme memory usage during compilation/linking
 * and extreme binary size.
 * 
 * This mechanism provides macros which solve this problem by effectively giving each
 * instatiation an identity, represented by a class whose signature does not include the
 * configuration options. In plain C++ code, the solution is as follows:
 * 
 * ```
 * template <typename Arg>
 * class MyClass {
 *     // Arg is a type derived from MyClassService, so option can be accessed like this:
 *     // Arg::Option1
 * public:
 *     class NestedClass {};
 *     void function(NestedClass n);
 * };
 * 
 * template <typename ...Options>
 * class MyClassService {
 *     AIPSTACK_OPTION_CONFIG_VALUE(MyClassOptions, Option1)
 *     
 *     template <typename Instance_self=MyClassService>
 *     using Instance = MyClass<Instance_self>;
 *     // The type alias above should instead be defined with a macro like this:
 *     //AIPSTACK_DEF_INSTANCE(MyClassService, MyClass)
 * };
 * 
 * // At the site of instantiation:
 * struct MyClassInstance_arg : public MyClassService<MyClassOptions::Option1::Is<5>> {};
 * using MyClassInstance = typename MyClassInstance_arg::template Instance<MyClassInstance_arg>;
 * // The struct and type alias above should instead be defined with a macro like this:
 * //AIPSTACK_MAKE_INSTANCE(MyClassInstance, (MyClassService<MyClassOptions::Option1::Is<5>>))
 * ```
 * 
 * The resulting `MyClassInstance` is a type alias for `MyClass<MyClassInstance_arg>`.
 * The expression of this type does not contain the configuration options, which are hidden
 * as the base class type of `MyClassInstance_arg`. When this mechanism is used consistently,
 * the explosion of symbol lengths is prevented.
 * 
 * @{
 */

/**
 * Define a template alias allowing definition of named instances using
 * @ref AIPSTACK_MAKE_INSTANCE.
 * 
 * See the @ref instance module description.
 * 
 * @param self Type which will be the base class of types passed to the `class` class
 *        template. This should be the name of the class where this macro is used.
 * @param class Class template which accepts a single type parameter, which will be a
 *        different trivial struct type for each instance, derived from `self`.
 */
#define AIPSTACK_DEF_INSTANCE(self, class) \
template <typename Instance_self=self> \
using Instance = class<Instance_self>;

/**
 * Define a named instance based on a previous @ref AIPSTACK_DEF_INSTANCE definition.
 * 
 * See the @ref instance module description.
 * 
 * @param service_name Name of the type alias which will represent the instantiated type.
 * @param arg_expr_parens Class type which contains the @ref AIPSTACK_DEF_INSTANCE
 *        definition. It must be specified within parentheses; this allows using commas
 *        between template parameters, which would otherwise be treated as delimiting
 *        macro arguments.
 */
#define AIPSTACK_MAKE_INSTANCE(service_name, arg_expr_parens) \
struct service_name##_arg : public AIPSTACK_REMOVE_PARENS arg_expr_parens {}; \
using service_name = typename service_name##_arg::template Instance<service_name##_arg>;

/** @} */

#endif
