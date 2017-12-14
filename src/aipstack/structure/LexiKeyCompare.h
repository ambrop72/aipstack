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

#ifndef AIPSTACK_LEXICOGRAPHICAL_KEY_COMPARE_H
#define AIPSTACK_LEXICOGRAPHICAL_KEY_COMPARE_H

#include <aipstack/meta/BasicMetaUtils.h>
#include <aipstack/meta/TypeListUtils.h>
#include <aipstack/meta/ListForEach.h>

namespace AIpStack {

// Fields should be MakeTypeList<WrapValue<FieldType, &ObjType::Field>, ...>
template <typename ObjType, typename Fields>
class LexiKeyCompare {
public:
    static int CompareKeys (ObjType const &op1, ObjType const &op2)
    {
        int result;

        auto equal = ListForBreak<Fields>([&](auto elem) {
            constexpr auto member = decltype(elem)::Type::Value;
            
            if (op1.*member < op2.*member) {
                result = -1;
                return false;
            }

            if (op2.*member < op1.*member) {
                result = 1;
                return false;
            }

            return true;
        });

        if (equal) {
            result = 0;
        }

        return result;
    }
    
    static bool KeysAreEqual (ObjType const &op1, ObjType const &op2)
    {
        return ListForBreak<Fields>([&](auto elem) {
            constexpr auto member = decltype(elem)::Type::Value;
            return op1.*member == op2.*member;
        });
    }
};

}

#endif
