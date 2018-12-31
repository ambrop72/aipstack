/*
 * Copyright (c) 2016 Ambroz Bizjak
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

#ifndef AIPSTACK_LINKED_LIST_H
#define AIPSTACK_LINKED_LIST_H

#include <type_traits>

#include <aipstack/misc/Assert.h>
#include <aipstack/structure/Accessor.h>

namespace AIpStack {

template <typename, typename, bool> class LinkedList;
template <typename, typename> class CircularLinkedList;

template <typename LinkModel>
class LinkedListNode {
    template <typename, typename, bool> friend class LinkedList;
    template <typename, typename> friend class CircularLinkedList;
    
    using Link = typename LinkModel::Link;
    
private:
    Link next;
    Link prev;
};

template <typename LinkModel, bool WithLast>
struct LinkedList__Extra_WithLast {};

template <typename LinkModel>
struct LinkedList__Extra_WithLast<LinkModel, true> {
    typename LinkModel::Link m_last;
};

template <
    typename Accessor,
    typename LinkModel,
    bool WithLast_
>
class LinkedList :
    private LinkedList__Extra_WithLast<LinkModel, WithLast_>
{
    using Link = typename LinkModel::Link;
    
private:
    Link m_first;
    
public:
    inline static constexpr bool WithLast = WithLast_;
    
    using State = typename LinkModel::State;
    using Ref = typename LinkModel::Ref;
    
    inline void init ()
    {
        m_first = Link::null();
    }
    
    inline bool isEmpty () const
    {
        return m_first.isNull();
    }
    
    inline Ref first (State st = State()) const
    {
        return m_first.ref(st);
    }
    
    template <bool Enable = WithLast, typename = std::enable_if_t<Enable>>
    inline Ref lastNotEmpty (State st = State()) const
    {
        AIPSTACK_ASSERT(!m_first.isNull());
        
        return this->m_last.ref(st);
    }
    
    inline static Ref next (Ref e, State st = State())
    {
        return ac(e).next.ref(st);
    }
    
    inline Ref prevNotFirst (Ref e, State st = State()) const
    {
        AIPSTACK_ASSERT(!m_first.isNull());
        AIPSTACK_ASSERT(!(e.link(st) == m_first));
        
        return ac(e).prev.ref(st);
    }
    
    void prepend (Ref e, State st = State())
    {
        ac(e).next = m_first;
        if (!m_first.isNull()) {
            ac(m_first.ref(st)).prev = e.link(st);
        } else {
            set_last(e.link(st));
        }
        m_first = e.link(st);
    }
    
    template <bool Enable = WithLast, typename = std::enable_if_t<Enable>>
    void append (Ref e, State st = State())
    {
        ac(e).next = Link::null();
        if (!m_first.isNull()) {
            ac(e).prev = this->m_last;
            ac(this->m_last.ref(st)).next = e.link(st);
        } else {
            m_first = e.link(st);
        }
        this->m_last = e.link(st);
    }
    
    void insertAfter (Ref e, Ref after_e, State st = State())
    {
        ac(e).prev = after_e.link(st);
        ac(e).next = ac(after_e).next;
        ac(after_e).next = e.link(st);
        if (!ac(e).next.isNull()) {
            ac(ac(e).next.ref(st)).prev = e.link(st);
        } else {
            set_last(e.link(st));
        }
    }
    
    void remove (Ref e, State st = State())
    {
        if (!(e.link(st) == m_first)) {
            ac(ac(e).prev.ref(st)).next = ac(e).next;
            if (!ac(e).next.isNull()) {
                ac(ac(e).next.ref(st)).prev = ac(e).prev;
            } else {
                set_last(ac(e).prev);
            }
        } else {
            m_first = ac(e).next;
        }
    }
    
    inline void removeFirst (State st = State())
    {
        AIPSTACK_ASSERT(!m_first.isNull());
        
        m_first = ac(m_first.ref(st)).next;
    }
    
    inline static void markRemoved (Ref e, State st = State())
    {
        ac(e).next = e.link(st);
    }
    
    inline static bool isRemoved (Ref e, State st = State())
    {
        return ac(e).next == e.link(st);
    }
    
private:
    inline static LinkedListNode<LinkModel> & ac (Ref ref)
    {
        return Accessor::access(*ref);
    }
    
    template <typename Dummy = std::true_type>
    inline void set_last (Link last, std::enable_if_t<WithLast, Dummy> = {})
    {
        this->m_last = last;
    }
    
    template <typename Dummy = std::true_type>
    inline void set_last (Link, std::enable_if_t<!WithLast, Dummy> = {})
    {}
};

template <
    typename Accessor,
    typename LinkModel
>
class CircularLinkedList
{
    using Link = typename LinkModel::Link;
    
public:
    using State = typename LinkModel::State;
    using Ref = typename LinkModel::Ref;
    
    CircularLinkedList () = delete;
    
    inline static void initLonely (Ref e, State st = State())
    {
        AIPSTACK_ASSERT(!e.isNull());
        
        ac(e).prev = e.link(st);
        ac(e).next = e.link(st);
    }

    inline static bool isLonely (Ref e, State st = State())
    {
        AIPSTACK_ASSERT(!e.isNull());

        return ac(e).next == e.link(st);
    }
    
    static void initAfter (Ref e, Ref other, State st = State())
    {
        AIPSTACK_ASSERT(!e.isNull());
        AIPSTACK_ASSERT(!other.isNull());
        
        ac(e).prev = other.link(st);
        ac(e).next = ac(other).next;
        ac(other).next = e.link(st);
        ac(ac(e).next.ref(st)).prev = e.link(st);
    }
    
    static void initBefore (Ref e, Ref other, State st = State())
    {
        AIPSTACK_ASSERT(!e.isNull());
        AIPSTACK_ASSERT(!other.isNull());
        
        ac(e).next = other.link(st);
        ac(e).prev = ac(other).prev;
        ac(other).prev = e.link(st);
        ac(ac(e).prev.ref(st)).next = e.link(st);
    }
    
    static void moveOtherNodesBefore (Ref e, Ref other, State st = State())
    {
        AIPSTACK_ASSERT(!e.isNull());
        AIPSTACK_ASSERT(!other.isNull());
        AIPSTACK_ASSERT(!(ac(e).next == other.link(st)));

        ac(ac(other).prev.ref(st)).next = ac(e).next;
        ac(ac(e).next.ref(st)).prev = ac(other).prev;
        
        ac(ac(e).prev.ref(st)).next = other.link(st);
        ac(other).prev = ac(e).prev;

        ac(e).prev = e.link(st);
        ac(e).next = e.link(st);
    }
    
    static void remove (Ref e, State st = State())
    {
        AIPSTACK_ASSERT(!e.isNull());
        
        ac(ac(e).prev.ref(st)).next = ac(e).next;
        ac(ac(e).next.ref(st)).prev = ac(e).prev;
    }
    
    inline static Ref prev (Ref e, State st = State())
    {
        AIPSTACK_ASSERT(!e.isNull());
        
        return ac(e).prev.ref(st);
    }
    
    inline static Ref next (Ref e, State st = State())
    {
        AIPSTACK_ASSERT(!e.isNull());
        
        return ac(e).next.ref(st);
    }
    
    inline static void markRemoved (Ref e, State st = State())
    {
        AIPSTACK_ASSERT(!e.isNull());
        (void)st;
        
        ac(e).next = Link::null();
    }
    
    inline static bool isRemoved (Ref e, State st = State())
    {
        AIPSTACK_ASSERT(!e.isNull());
        (void)st;
        
        return ac(e).next.isNull();
    }
    
private:
    inline static LinkedListNode<LinkModel> & ac (Ref ref)
    {
        return Accessor::access(*ref);
    }
};

}

#endif
