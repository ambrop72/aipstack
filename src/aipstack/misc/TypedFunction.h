/*
 * Copyright (c) 2019 Ambroz Bizjak
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

#ifndef AIPSTACK_TYPED_FUNCTION_H
#define AIPSTACK_TYPED_FUNCTION_H

#include <type_traits>
#include <utility>

namespace AIpStack {
/**
 * @addtogroup misc
 * @{
 */

#ifndef IN_DOXYGEN

namespace TypedFunctionPrivate {
    // Internal template used to implement TypedFunctionFuncType.
    template<typename Impl, typename Void>
    struct GetFuncTypeImpl {
        static_assert(!std::is_same_v<Impl, Impl>,
            "This type is not supported by TypedFunction.");
    };
    
    // Implementation for function pointers.
    template<typename RetType, typename ...ArgTypes>
    struct GetFuncTypeImpl<RetType (*)(ArgTypes...), void> {
        using Type = RetType(ArgTypes...);
    };

    // Implementation for classes with operator() const.
    template<typename ClassType>
    struct GetFuncTypeImpl<ClassType, std::void_t<decltype(&ClassType::operator())>> {
        template<typename>
        struct ExtractFuncType;
        
        template<typename FuncClassType, typename RetType, typename ...ArgTypes>
        struct ExtractFuncType<RetType (FuncClassType::*)(ArgTypes...) const> {
            using Type = RetType(ArgTypes...);
        };
    
        using Type = typename ExtractFuncType<decltype(&ClassType::operator())>::Type;
    };
}

template<typename FuncType, typename Impl>
class TypedFunction;

#endif

/**
 * Deduce the function type from a callable type for use with @ref
 * TypedFunction<RetType(ArgTypes...), Impl> "TypedFunction".
 * 
 * The result is a function type (e.g. `void(int, float)`) determined by deducing the
 * return and argument types from the `Impl` type.
 * 
 * @tparam Impl Callable type for which to get the function type. It must satisfy
 *         the requirements of TypedFunction.
 */
template<typename Impl>
using TypedFunctionFuncType =
    typename TypedFunctionPrivate::GetFuncTypeImpl<Impl, void>::Type;

/**
 * Type-preserving non-polymorphic function wrapper.
 * 
 * An instance of this class represents a function with a specific return type and
 * argument types. Unlike `std::function`, this class does not hide the type of the
 * underlying callable object, which is preserved in the form of the `Impl` template
 * parameter. Consequently it does not have the associated performance impact and
 * does not allocate memory itself.
 * 
 * This class is meant to be used in template functions that take some kind of callback
 * argument. Unlike the common pattern in C++ where the type of the function object is
 * not constrained by the type system, this allows deducing and/or restricting the
 * return type and argument types. For example:
 * 
 * ```
 * template<typename CallbackImpl>
 * void someFunction(TypedFunction<int(bool), CallbackImpl> callback)
 * {
 *      ...
 *      int result = callback(true);
 *      ...
 * }
 * ```
 * 
 * The preferred method of creating a new TypedFunction object is by calling the @ref
 * makeTypedFunction function providing a callable object. The callable object will
 * be stored in the resulting TypedFunction object and the `Impl` template parameter
 * of TypedFunction will reflect the type of the stored callable. For example:
 * 
 * ```
 * makeTypedFunction([](bool arg) { return arg ? 1 : 2; })
 * ```
 * 
 * Once a TypedFunction object is constructed, @ref operator()(ArgTypes...) const can
 * be used to call the stored `Impl` object.
 * 
 * The `Impl` type may be one of the following:
 * - A function pointer type with matching return and argument types, that is
 *   `RetType (*)(ArgTypes...)`.
 * - A class type (including lambda) which has a const call operator with matching
 *   return and argument types; formally, where `decltype(&Impl::operator())` has
 *   type `RetType (C::*)(ArgTypes...)` for some type `C`.
 * 
 * If `Impl` does not specify the requirements above, a compile error will result.
 * Note particularly that the return and argument types of `Impl` must match the
 * template parameters `RetType` and `ArgTypes`.
 * 
 * @tparam RetType Return type.
 * @tparam ArgTypes Argument types.
 * @tparam Impl Type of stored callable object (see the class description).
 */
template<typename RetType, typename ...ArgTypes, typename Impl>
class TypedFunction<RetType(ArgTypes...), Impl>
{
    static_assert(std::is_same_v<TypedFunctionFuncType<Impl>, RetType(ArgTypes...)>);
    
public:
    /**
     * Construct a TypedFunction object storing the specified callable object.
     * 
     * @param impl The callable object to store. It will be moved (if possible) or
     *        copied into the TypedFunction object.
     */
    constexpr TypedFunction(Impl impl) :
        m_impl(std::move(impl))
    {}
    
    /**
     * Call the stored callable object.
     * 
     * This calls the stored object using an expression like
     * `impl(std::forward<ArgTypes>(args)...)`.
     * 
     * @param args Arguments.
     * @return Return value.
     */
    constexpr RetType operator()(ArgTypes... args) const
    {
        return m_impl(std::forward<ArgTypes>(args)...);
    }
    
private:
    Impl const m_impl;
};

/**
 * Utility function for constructing @ref TypedFunction<RetType(ArgTypes...), Impl>
 * "TypedFunction" from a callable object.
 * 
 * This function automatically deduces the return and argument from the `Impl` type.
 * 
 * @tparam Impl Type of callable object to be stored within TypedFunction. It must
 *         satisfy the requirements of TypedFunction.
 * @param impl The callable object to store. It will be moved (if possible) or
 *        copied into the TypedFunction object.
 */
template<typename Impl>
constexpr auto makeTypedFunction(Impl impl) ->
    TypedFunction<TypedFunctionFuncType<Impl>, Impl>
{
    return TypedFunction<TypedFunctionFuncType<Impl>, Impl>(std::move(impl));
}

/** @} */
}

#endif
