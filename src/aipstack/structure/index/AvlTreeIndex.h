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

#ifndef AIPSTACK_AVL_TREE_INDEX_H
#define AIPSTACK_AVL_TREE_INDEX_H

#include <type_traits>
#include <functional>

#include <aipstack/misc/Use.h>
#include <aipstack/misc/Assert.h>
#include <aipstack/structure/AvlTree.h>
#include <aipstack/structure/TreeCompare.h>
#include <aipstack/structure/Accessor.h>
#include <aipstack/infra/Instance.h>

namespace AIpStack {

template <typename Arg>
class AvlTreeIndex {
    AIPSTACK_USE_TYPES(Arg, (HookAccessor, LookupKeyArg, KeyFuncs, LinkModel))
    AIPSTACK_USE_VALS(Arg, (Duplicates))
    
    AIPSTACK_USE_TYPES(LinkModel, (State, Ref))
    
    using TreeNode = AvlTreeNode<LinkModel>;
    
public:
    class Node {
        friend AvlTreeIndex;
        
        TreeNode tree_node;
    };
    
    class Index {
        using TreeNodeAccessor = ComposedAccessor<
            HookAccessor,
            MemberAccessor<Node, TreeNode, &Node::tree_node>
        >;
        
        // This is used when Duplicates=true, in which case it extends the key with a
        // pointer to the Node so that no two entries can compare equal. Note that we give
        // this as the KeyFunc to the AvlTree but we still directly use the original
        // KeyFuncs in findFirst and findNext.
        struct MultiKeyFuncs {
            template <typename BaseKey>
            struct EntryKey {
                BaseKey base_key;
                Node *ptr;
            };
            
            template <typename Entry>
            inline static auto GetKeyOfEntry (Entry &e) ->
                EntryKey<decltype(KeyFuncs::GetKeyOfEntry(e))>
            {
                return EntryKey<decltype(KeyFuncs::GetKeyOfEntry(e))>{
                    KeyFuncs::GetKeyOfEntry(e),
                    &HookAccessor::access(e)
                };
            }
            
            template <typename BaseKey1, typename BaseKey2>
            static int CompareKeys (EntryKey<BaseKey1> k1, EntryKey<BaseKey2> k2)
            {
                // Comparing two entries: use lexicographical order by the base key
                // followed by the Node pointer (which identifies the entry).
                int base_cmp = KeyFuncs::CompareKeys(k1.base_key, k2.base_key);
                if (base_cmp != 0) {
                    return base_cmp;
                }
                return std::less<Node *>()(k1.ptr, k2.ptr) ? -1 : 
                       std::less<Node *>()(k2.ptr, k1.ptr) ? 1 : 0;
            }
            
            template <typename BaseKey2>
            static int CompareKeys (LookupKeyArg k1, EntryKey<BaseKey2> k2)
            {
                // Comparing a lookup key and an entry (as part of a lookup): use
                // lexicographical order as if the key had a Node pointer that is less then
                // all real pointers. This means that the lookup key is ordered just before
                // all entries which have a key equal to that lookup key.
                int base_cmp = KeyFuncs::CompareKeys(k1, k2.base_key);
                if (base_cmp != 0) {
                    return base_cmp;
                }
                return -1;
            }
        };
        
        // If duplicates are allowed the tree uses MultiKeyFuncs so that from the
        // perspective of the tree no two entries ever compare equal. Otherwise it uses the
        // original KeyFuncs provided by the user.
        using TreeKeyFuncs = std::conditional_t<Duplicates, MultiKeyFuncs, KeyFuncs>;
        
        struct TheTreeCompare : public TreeCompare<LinkModel, TreeKeyFuncs> {};
        
        using EntryTree = AvlTree<TreeNodeAccessor, TheTreeCompare, LinkModel>;
        
    public:
        inline void init ()
        {
            m_tree.init();
        }
        
        inline void addEntry (Ref e, State st = State())
        {
            bool inserted = m_tree.insert(e, nullptr, st);
            AIPSTACK_ASSERT(inserted)
            (void)inserted;
        }
        
        inline void removeEntry (Ref e, State st = State())
        {
            m_tree.remove(e, st);
        }
        
        template <bool Enable = !Duplicates, typename = std::enable_if_t<Enable>>
        inline Ref findEntry (LookupKeyArg key, State st = State()) const
        {
            Ref entry = m_tree.template lookup<LookupKeyArg>(key, st);
            AIPSTACK_ASSERT(entry.isNull() ||
                KeyFuncs::KeysAreEqual(key, KeyFuncs::GetKeyOfEntry(*entry)))
            return entry;
        }
        
        template <bool Enable = Duplicates, typename = std::enable_if_t<Enable>>
        inline Ref findFirst (LookupKeyArg key, State st = State()) const
        {
            int cmpKeyEntry;
            Ref entry = m_tree.template lookupInexact<LookupKeyArg>(key, cmpKeyEntry, st);
            
            if (!entry.isNull()) {
                // The `entry` returned by lookupInexact can be an exact match for `key`
                // (cmpKeyEntry==0), the greatest entry less than `key` (cmpKeyEntry>0) or
                // the smallest entry greater than `key` (cmpKeyEntry<0). In our case it
                // cannot be an exact match since MultiKeyFuncs never considers a lookup
                // key equal to an entry.
                AIPSTACK_ASSERT(cmpKeyEntry != 0)

                // If we got an entry that is less than `key`, advance to the next entry in
                // order to obtain the smallest entry greater than `key` (if any).
                if (cmpKeyEntry > 0) {
                    entry = m_tree.next(entry, st);
                }
                
                // If `entry` is an entry which does not match the `key`, then there are
                // no entries with that key so return null. Otherwise this is the first
                // entry with that key.
                if (!entry.isNull() &&
                    !KeyFuncs::KeysAreEqual(key, KeyFuncs::GetKeyOfEntry(*entry)))
                {
                    entry = Ref::null();
                }
            }
            
            return entry;
        }
        
        template <bool Enable = Duplicates, typename = std::enable_if_t<Enable>>
        inline Ref findNext (LookupKeyArg key, Ref prev_e, State st = State()) const
        {
            // Move to the next entry and check if it matches the `key`. If not then this
            // was the last entry with that key so return null.
            Ref entry = m_tree.next(prev_e, st);
            if (!entry.isNull() &&
                !KeyFuncs::KeysAreEqual(key, KeyFuncs::GetKeyOfEntry(*entry)))
            {
                entry = Ref::null();
            }
            
            return entry;
        }

        inline bool isEmpty () const
        {
            return m_tree.isEmpty();
        }
        
        inline Ref first (State st = State()) const
        {
            return m_tree.first(st);
        }
        
        inline Ref next (Ref node, State st = State()) const
        {
            return m_tree.next(node, st);
        }
        
    private:
        EntryTree m_tree;
    };
};

struct AvlTreeIndexService {
    template <typename HookAccessor_, typename LookupKeyArg_,
              typename KeyFuncs_, typename LinkModel_, bool Duplicates_>
    struct Index {
        using HookAccessor = HookAccessor_;
        using LookupKeyArg = LookupKeyArg_;
        using KeyFuncs = KeyFuncs_;
        using LinkModel = LinkModel_;
        static bool const Duplicates = Duplicates_;
        AIPSTACK_DEF_INSTANCE(Index, AvlTreeIndex)
    };
};

}

#endif
