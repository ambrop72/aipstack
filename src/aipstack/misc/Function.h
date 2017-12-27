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
public:
    inline Function () noexcept :
        m_func_ptr(nullptr),
        m_storage{}
    {}

    template <typename Callable>
    Function (Callable const &callable) noexcept
    {
        static_assert(sizeof(Callable) <= FunctionStorageSize,
                      "Callable too large (greater than FunctionStorageSize)");
        static_assert(std::is_trivially_copyable<Callable>::value,
                      "Callable not trivially copyable");
        static_assert(std::is_trivially_destructible<Callable>::value,
                      "Callable not trivially destructible");

        m_func_ptr = &trampoline<Callable>;

        if (std::is_empty<Callable>::value) {
            std::memset(m_storage, 0, FunctionStorageSize);
        } else {
            std::memcpy(m_storage, std::addressof(callable), sizeof(Callable));

            if (sizeof(Callable) < FunctionStorageSize) {
                std::memset(m_storage + sizeof(Callable), 0,
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
    using FunctionPointerType = Ret (*) (void const *, Args...);

    template <typename Callable>
    static Ret trampoline (void const *param, Args ...args)
    {
        Callable const *c = static_cast<Callable const *>(param);
        return (*c)(std::forward<Args>(args)...);
    }

private:
    FunctionPointerType m_func_ptr;
    alignas(alignof(std::max_align_t)) char m_storage[FunctionStorageSize];
};

template <typename Callable>
inline std::reference_wrapper<Callable const> RefFunc (Callable const &callable) noexcept
{
    return std::reference_wrapper<Callable const>(callable);
}

}

#endif
