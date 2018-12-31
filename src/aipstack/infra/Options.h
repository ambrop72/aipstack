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

#ifndef AIPSTACK_OPTIONS_H
#define AIPSTACK_OPTIONS_H

#include <aipstack/meta/BasicMetaUtils.h>
#include <aipstack/meta/TypeListUtils.h>
#include <aipstack/meta/TypeDict.h>

namespace AIpStack {

/**
 * @ingroup infra
 * @defgroup configuration Static Configuration Options
 * @brief Static configuration system based on templates
 * 
 * This is a simple static configuration system which allows a module to define
 * configuration options and be instantiated with specific values of these options. Each
 * option is defined either as a "type" option (the option value will be a C++ type) or a
 * "value" option with a specific value type (the option value will be a value of that
 * type).
 * 
 * The @ref ConfigOptionType class represents a "type" option and the
 * @ref ConfigOptionValue class represents a "value" option; these encode the type of option
 * and its default value. Actual options for a module are declared in an "options" struct
 * corresponding to the module, as classes derived from these ConfigOption* classes,
 * effectively giving each option a name and identity. These derived classes should be
 * declared using the macros @ref AIPSTACK_OPTION_DECL_TYPE and
 * @ref AIPSTACK_OPTION_DECL_VALUE.
 * 
 * Below is an example of declaring options for a module; the last arguments to macros are
 * the default values.
 * 
 * ```
 * struct MyModuleOptions {
 *     AIPSTACK_OPTION_DECL_TYPE(ExampleTypeOption, int)
 *     AIPSTACK_OPTION_DECL_VALUE(ExampleValueOption, bool, false)
 * };
 * ```
 * 
 * After the options class is declared, a class template for the configurable module itself
 * is defined, which must accept a type parameter pack named `Options` representing option
 * value assignments. Within this class, the macros @ref AIPSTACK_OPTION_CONFIG_TYPE and
 * @ref AIPSTACK_OPTION_CONFIG_VALUE are used to retrieve the effective option values. An
 * example is shown below.
 *
 * ```
 * template <typename ...Options>
 * class MyModule {
 * public:
 *     AIPSTACK_OPTION_CONFIG_TYPE(MyModuleOptions, ExampleTypeOption)
 *     AIPSTACK_OPTION_CONFIG_VALUE(MyModuleOptions, ExampleValueOption)
 * 
 *     MyModule () {
 *         using Type = ExampleTypeOption; // double, if instantiated as below
 *         constexpr bool value = ExampleValueOption; // true, if instantiated as below
 *     }
 * };
 * ```
 * 
 * Users of the configurable module would instantiate this class template by passing zero or
 * more @ref ConfigOptionType::Is or @ref ConfigOptionValue::Is type expressions as template
 * parameters. If an option is not included it will have the default value specified in its
 * declaration, and if it is specified multiple times then the last value will be effective.
 * An example is shown below.
 * 
 * ```
 * using MyModuleInstance = MyModule<
 *     MyModuleOptions::ExampleTypeOption::Is<double>,
 *     MyModuleOptions::ExampleValueOption::Is<true>
 * >;
 * ```
 * 
 * @{
 */

#ifndef IN_DOXYGEN

namespace OptionsPrivate {
    template <typename Derived, typename DefaultValue, typename ...Options>
    using GetValue = TypeDictGetOrDefault<
        TypeListReverse<MakeTypeList<Options...>>, Derived, DefaultValue
    >;
}

#endif

/**
 * Represents a "value" static configuration option.
 * 
 * @tparam Derived The derived class identifying this option.
 * @tparam ValueType The type of the option value.
 * @tparam DefaultValue The default value if none is provided.
 */
template <typename Derived, typename ValueType, ValueType DefaultValue>
class ConfigOptionValue {
public:
    /**
     * Provide a value for the option.
     * 
     * Types obtained via this alias are passed as one of the variadic template
     * parameters to various "Config" template classes in order to specify the
     * value of an option.
     * 
     * @tparam Value The desired value for the option.
     */
    template <ValueType Value>
    using Is = TypeDictEntry<Derived, WrapValue<ValueType, Value>>;
    
#ifndef IN_DOXYGEN
    template <typename ...Options>
    struct Config {
        inline static constexpr ValueType Value = OptionsPrivate::GetValue<
            Derived, WrapValue<ValueType, DefaultValue>, Options...>::Value;
    };
#endif
};

/**
 * Represents a "type" static configuration option.
 * 
 * @tparam Derived The derived class identifying this option.
 * @tparam DefaultValue The default value if none is provided.
 */
template <typename Derived, typename DefaultValue>
class ConfigOptionType {
public:
    /**
     * Provide a value for the option.
     * 
     * Types obtained via this alias are passed as one of the variadic template
     * parameters to various "Config" template classes in order to specify the
     * value of an option.
     * 
     * @tparam Value The desired value for the option.
     */
    template <typename Value>
    using Is = TypeDictEntry<Derived, Value>;
    
#ifndef IN_DOXYGEN
    template <typename ...Options>
    struct Config {
        using Value = OptionsPrivate::GetValue<Derived, DefaultValue, Options...>;
    };
#endif
};

/**
 * Declare a "value" static configuration option.
 * 
 * See the \ref configuration module description.
 * 
 * @param name Name of the derived class representing the option.
 * @param type Value type for the option. Must be a type that can be used for non-type
 *             template parameters.
 * @param default Default value of the option (a value of type `type`).
 */
#define AIPSTACK_OPTION_DECL_VALUE(name, type, default) \
class name : public AIpStack::ConfigOptionValue<name, type, default> {};

/**
 * Declare a "type" static configuration option.
 * 
 * See the \ref configuration module description.
 * 
 * @param name Name of the derived class representing the option.
 * @param default Default value of the option (a type).
 */
#define AIPSTACK_OPTION_DECL_TYPE(name, default) \
class name : public AIpStack::ConfigOptionType<name, default> {};

/**
 * Retrieve and expose the value of a "value" configuration option.
 * 
 * See the \ref configuration module description.
 * 
 * @param decls The options class type where the options are declared.
 * @param name The name of the option as declared in the options class.
 */
#define AIPSTACK_OPTION_CONFIG_VALUE(decls, name) \
inline static constexpr auto name = decls::name::Config<Options...>::Value;

/**
 * Retrieve and expose the value of a "type" configuration option.
 * 
 * See the \ref configuration module description.
 * 
 * @param decls The options class type where the options are declared.
 * @param name The name of the option as declared in the options class.
 */
#define AIPSTACK_OPTION_CONFIG_TYPE(decls, name) \
using name = typename decls::name::Config<Options...>::Value;

/** @} */

}

#endif
