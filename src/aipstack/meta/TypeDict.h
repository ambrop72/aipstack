/*
 * Copyright (c) 2015 Ambroz Bizjak
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

#ifndef AIPSTACK_TYPE_DICT_H
#define AIPSTACK_TYPE_DICT_H

#include <type_traits>

#include <aipstack/meta/TypeList.h>

namespace AIpStack {

/**
 * @addtogroup meta
 * @{
 */

template <typename TKey, typename TValue>
struct TypeDictEntry {
    using Key = TKey;
    using Value = TValue;
};

template <typename TResult>
struct TypeDictFound {
    inline static constexpr bool Found = true;
    using Result = TResult;
};

struct TypeDictNotFound {
    inline static constexpr bool Found = false;
};

#ifndef IN_DOXYGEN

namespace Private {
    template <typename Key, typename EntriesList>
    struct TypeDictFindHelper;
    
    template <typename Key>
    struct TypeDictFindHelper<Key, EmptyTypeList> {
        using Result = TypeDictNotFound;
    };
    
    template <typename Key, typename Value, typename Tail>
    struct TypeDictFindHelper<Key, ConsTypeList<TypeDictEntry<Key, Value>, Tail>> {
        using Result = TypeDictFound<Value>;
    };
    
    template <typename Key, typename OtherKey, typename Value, typename Tail>
    struct TypeDictFindHelper<Key, ConsTypeList<TypeDictEntry<OtherKey, Value>, Tail>> {
        using Result = typename TypeDictFindHelper<Key, Tail>::Result;
    };
}

#endif

template <typename EntriesList, typename Key>
using TypeDictFind =
#ifdef IN_DOXYGEN
implementation_hidden;
#else
typename Private::TypeDictFindHelper<Key, EntriesList>::Result;
#endif

#ifndef IN_DOXYGEN

namespace Private {
    template <typename Default, typename FindResult>
    struct TypeDictDefaultHelper {
        using Result = typename FindResult::Result;
    };
    
    template <typename Default>
    struct TypeDictDefaultHelper<Default, TypeDictNotFound> {
        using Result = Default;
    };
}

#endif

template <typename EntriesList, typename Key, typename Default>
using TypeDictGetOrDefault =
#ifdef IN_DOXYGEN
implementation_hidden;
#else
typename Private::template TypeDictDefaultHelper<Default, TypeDictFind<EntriesList, Key>>::Result;
#endif

/** @} */

}

#endif
