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

#ifndef AIPSTACK_NONCOPYABLE_H
#define AIPSTACK_NONCOPYABLE_H

namespace AIpStack {

/**
 * @addtogroup misc
 * @{
 */

/**
 * Inheriting this class makes the derived class non-copyable.
 * 
 * This class has a deleted copy-constructor and copy-assignment operator. A derived class
 * will also have those deleted and consequently also a deleted move-constructor and
 * move-assignment operator (so long as these are not defined explicitly).
 * 
 * @tparam Derived Dummy template parameter to avoid problems with ambiguous base classes.
 *         It is recommended to use the type of the derived class.
 */
template <typename Derived = void>
class NonCopyable {
public:
    /**
     * Default constructor, does nothing.
     */
    NonCopyable () = default;
    
    /**
     * Deleted copy-constructor.
     */
    NonCopyable (NonCopyable const &) = delete;
    
    /**
     * Deleted copy-assignment operator.
     */
    NonCopyable & operator= (NonCopyable const &) = delete;
};

/** @} */

}

#endif
