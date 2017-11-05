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

#ifndef AIPSTACK_RESOURCE_TUPLE_H
#define AIPSTACK_RESOURCE_TUPLE_H

#include <stddef.h>

#include <type_traits>
#include <utility>
#include <tuple>

#include <aipstack/meta/BasicMetaUtils.h>

namespace AIpStack {

/**
 * @addtogroup misc
 * @{
 */

/**
 * Dummy class used to select a @ref ResourceTuple constructor.
 */
struct ResourceTupleInitSame {};

#ifndef IN_DOXYGEN

namespace ResourceTuplePrivate {
    template <typename Elem, size_t Index>
    class InheritElemHelper
    {
    public:
        Elem m_elem;
        
    public:
        InheritElemHelper () = default;
        
        template <typename... Args>
        inline InheritElemHelper (ResourceTupleInitSame, Args && ... args) :
            m_elem(std::forward<Args>(args)...)
        {
        }
    };
    
    template <typename ElemsSequence, typename IndicesSequence>
    class InheritAllHelper;
    
    template <typename... Elems, size_t... Indices>
    class InheritAllHelper<std::tuple<Elems...>, std::index_sequence<Indices...>> :
        public InheritElemHelper<Elems, Indices>...
    {
    public:
        InheritAllHelper () = default;
        
        template <typename... Args>
        inline InheritAllHelper (ResourceTupleInitSame, Args const & ... args) :
            InheritElemHelper<Elems, Indices>{ResourceTupleInitSame(), args...}...
        {
        }
    };
    
    template <typename... Elems>
    using InheritAllAlias = InheritAllHelper<
        std::tuple<Elems...>, std::make_index_sequence<sizeof...(Elems)>>;
}

#endif

/**
 * Simple tuple container.
 * 
 * This class contains one object corresponding to each element of the `Elems` parameter
 * pack. It allows constructing all elements using the same arguments. This is why this
 * class exists as opposed to using `std::tuple`.
 * 
 * @tparam Elems Types of tuple elements.
 */
template <typename... Elems>
class ResourceTuple
#ifndef IN_DOXYGEN
    :private ResourceTuplePrivate::InheritAllAlias<Elems...>
#endif
{
    using InheritAll = ResourceTuplePrivate::InheritAllAlias<Elems...>;
    
public:
    /**
     * Get the type of the element at the given index.
     * 
     * @tparam Index Element index. Must be less than `sizeof...(Elems)`.
     */
    template <size_t Index>
    using ElemType = std::tuple_element_t<Index, std::tuple<Elems...>>;
    
private:
    template <size_t Index>
    using ElemHelperType = ResourceTuplePrivate::InheritElemHelper<ElemType<Index>, Index>;
    
public:
    /**
     * Default constructor (defaulted), default-constructs the elements.
     */
    ResourceTuple () = default;
    
    /**
     * Construct tuple elements using the given construction arguments for each element.
     * 
     * @tparam Args Types of arguments used for constructing elements.
     * @param args Arguments used for constructing each element (all these are used for each
     *        element). Note that they are given by and passed to element constructors by
     *        const reference.
     */
    template <typename... Args>
    ResourceTuple (ResourceTupleInitSame, Args const & ... args) :
        InheritAll(ResourceTupleInitSame(), args...)
    {
    }
    
    /**
     * Return a reference to the element at the given index (non-const).
     * 
     * @tparam Index Element index. Must be less than `sizeof...(Elems)`.
     * @return Reference to the element at index `Index`.
     */
    template <size_t Index>
    ElemType<Index> & get (WrapSize<Index> = WrapSize<Index>())
    {
        return static_cast<ElemHelperType<Index> &>(*this).m_elem;
    }
    
    /**
     * Return a reference to the element at the given index (const).
     * 
     * @tparam Index Element index. Must be less than `sizeof...(Elems)`.
     * @return Reference to the element at index `Index`.
     */
    template <size_t Index>
    ElemType<Index> const & get (WrapSize<Index> = WrapSize<Index>()) const
    {
        return static_cast<ElemHelperType<Index> const &>(*this).m_elem;
    }
};

/** @} */

}

#endif
