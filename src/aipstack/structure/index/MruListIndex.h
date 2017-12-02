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

#ifndef AIPSTACK_MRU_LIST_INDEX_H
#define AIPSTACK_MRU_LIST_INDEX_H

#include <type_traits>

#include <aipstack/misc/Use.h>
#include <aipstack/structure/LinkedList.h>
#include <aipstack/structure/Accessor.h>
#include <aipstack/infra/Instance.h>

namespace AIpStack {

template <typename Arg>
class MruListIndex {
    AIPSTACK_USE_TYPES(Arg, (HookAccessor, LookupKeyArg, KeyFuncs, LinkModel))
    AIPSTACK_USE_VALS(Arg, (Duplicates))
    
    AIPSTACK_USE_TYPES(LinkModel, (State, Ref))
    
    using ListNode = LinkedListNode<LinkModel>;
    
public:
    class Node {
        friend MruListIndex;
        
        ListNode list_node;
    };
    
    class Index {
        using ListNodeAccessor = ComposedAccessor<
            HookAccessor,
            MemberAccessor<Node, ListNode, &Node::list_node>
        >;
        using EntryList = LinkedList<ListNodeAccessor, LinkModel, false>;
        
    public:
        inline void init ()
        {
            m_list.init();
        }
        
        inline void addEntry (Ref e, State st = State())
        {
            m_list.prepend(e, st);
        }
        
        inline void removeEntry (Ref e, State st = State())
        {
            m_list.remove(e, st);
        }
        
        template <typename Dummy = std::true_type>
        Ref findEntry (LookupKeyArg key, State st = State(),
                       std::enable_if_t<!Duplicates, Dummy> = std::true_type())
        {
            for (Ref e = m_list.first(st); !e.isNull(); e = m_list.next(e, st)) {
                if (KeyFuncs::KeysAreEqual(KeyFuncs::GetKeyOfEntry(*e), key)) {
                    if (!(e == m_list.first(st))) {
                        m_list.remove(e, st);
                        m_list.prepend(e, st);
                    }
                    return e;
                }
            }
            return Ref::null();
        }
        
        template <typename Dummy = std::true_type>
        inline Ref findFirst (LookupKeyArg key, State st = State(),
                              std::enable_if_t<Duplicates, Dummy> = std::true_type())
        {
            return findFirstNextCommon(key, m_list.first(st), st);
        }
        
        template <typename Dummy = std::true_type>
        inline Ref findNext (LookupKeyArg key, Ref prev_e, State st = State(),
                             std::enable_if_t<Duplicates, Dummy> = std::true_type())
        {
            return findFirstNextCommon(key, m_list.next(prev_e, st), st);
        }
        
        inline Ref first (State st = State())
        {
            return m_list.first(st);
        }
        
        inline Ref next (Ref node, State st = State())
        {
            return m_list.next(node, st);
        }
        
    private:
        Ref findFirstNextCommon (LookupKeyArg key, Ref start, State st)
        {
            for (Ref e = start; !e.isNull(); e = m_list.next(e, st)) {
                if (KeyFuncs::KeysAreEqual(KeyFuncs::GetKeyOfEntry(*e), key)) {
                    return e;
                }
            }
            return Ref::null();
        }
        
    private:
        EntryList m_list;
    };
};

struct MruListIndexService {
    template <typename HookAccessor_, typename LookupKeyArg_,
              typename KeyFuncs_, typename LinkModel_, bool Duplicates_>
    struct Index {
        using HookAccessor = HookAccessor_;
        using LookupKeyArg = LookupKeyArg_;
        using KeyFuncs = KeyFuncs_;
        using LinkModel = LinkModel_;
        static bool const Duplicates = Duplicates_;
        AIPSTACK_DEF_INSTANCE(Index, MruListIndex)
    };
};

}

#endif
