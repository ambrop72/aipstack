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

#ifndef AIPSTACK_FUNCTION_H
#define AIPSTACK_FUNCTION_H

#include <cstddef>
#include <cstring>

#include <type_traits>
#include <functional>
#include <memory>
#include <utility>

namespace AIpStack {

static constexpr std::size_t FunctionStorageSize = sizeof(void *);

template <typename>
class Function;

template <typename Ret, typename ...Args>
class Function<Ret(Args...)>
{
    struct Storage {
        alignas(alignof(std::max_align_t)) char data[FunctionStorageSize];
    };

    using FunctionPointerType = Ret (*) (Storage, Args...);

public:
    inline Function () noexcept :
        m_func_ptr(nullptr),
        m_storage{}
    {}

    template <typename Callable>
    Function (Callable callable) noexcept
    {
        static_assert(sizeof(Callable) <= FunctionStorageSize,
                      "Callable too large (greater than FunctionStorageSize)");
        static_assert(std::is_trivially_copy_constructible<Callable>::value,
                      "Callable not trivially copy constructible");
        static_assert(std::is_trivially_destructible<Callable>::value,
                      "Callable not trivially destructible");

        m_func_ptr = &trampoline<Callable>;

        if (std::is_empty<Callable>::value) {
            m_storage = Storage{};
        } else {
            std::memcpy(m_storage.data, std::addressof(callable), sizeof(Callable));

            if (sizeof(Callable) < FunctionStorageSize) {
                std::memset(m_storage.data + sizeof(Callable), 0,
                            FunctionStorageSize - sizeof(Callable));
            }
        }
    }

    inline explicit operator bool () const noexcept
    {
        return m_func_ptr != nullptr;
    }

    inline Ret operator() (Args ...args) const
    {
        return (*m_func_ptr)(m_storage, std::forward<Args>(args)...);
    }

private:
    template <typename Callable>
    static Ret trampoline (Storage storage, Args ...args)
    {
        Callable const *c = reinterpret_cast<Callable const *>(storage.data);
        return (*c)(std::forward<Args>(args)...);
    }

private:
    FunctionPointerType m_func_ptr;
    Storage m_storage;
};

template <typename Callable>
inline std::reference_wrapper<Callable const> RefFunc (Callable const &callable) noexcept
{
    return std::reference_wrapper<Callable const>(callable);
}

}

#endif
