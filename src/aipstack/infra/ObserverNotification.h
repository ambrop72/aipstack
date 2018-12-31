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

#ifndef AIPSTACK_OBSERVER_NOTIFICATION_H
#define AIPSTACK_OBSERVER_NOTIFICATION_H

#include <aipstack/misc/Assert.h>
#include <aipstack/misc/NonCopyable.h>

namespace AIpStack {

/**
 * @ingroup infra
 * @defgroup observer Observer Notification
 * @brief Provides mechanisms for application of the observer pattern
 * 
 * The @ref Observable and @ref Observer classes together implement the observer pattern,
 * excluding the specific mechanism used for notification calls. This includes association
 * and disassociation of observers with observables and modification-safe iteration of
 * observers for notifications.
 * 
 * Observers are associated with an observable using @ref Observable::addObserver and
 * disassociated using @ref Observer::reset or the Observer::~Observer destructor.
 * Notification of observers associated with an observable is performed using
 * @ref Observable::notifyKeepObservers or @ref Observable::notifyRemoveObservers (in the
 * latter case each observer is disassociated prior to being notified).
 * 
 * The @ref Observable and @ref Observer are not copy/move-constructible or
 * copy/move-assignable by design.
 * 
 * @{
 */

#ifndef IN_DOXYGEN

class ObserverNotificationPrivate
{
    struct ListNode {
        ListNode **m_prev;
        ListNode *m_next;
    };
    
public:
    class BaseObserver;
    
    class BaseObservable :
        private NonCopyable<BaseObservable>
    {
        friend BaseObserver;
        
    private:
        ListNode *m_first;
        
    public:
        inline BaseObservable () :
            m_first(nullptr)
        {}
        
        inline ~BaseObservable ()
        {
            reset();
        }
        
        inline bool hasObservers () const
        {
            return m_first != nullptr;
        }
        
        void reset ()
        {
            for (ListNode *node = m_first; node != nullptr; node = node->m_next) {
                AIPSTACK_ASSERT(node->m_prev != nullptr);
                node->m_prev = nullptr;
            }
            m_first = nullptr;
        }
        
        template <typename EnumerateFunc>
        void enumerateObservers (EnumerateFunc enumerate)
        {
            for (ListNode *node = m_first; node != nullptr; node = node->m_next) {
                enumerate(static_cast<BaseObserver &>(*node));
            }
        }
        
        template <bool RemoveNotified, typename NotifyFunc>
        void notifyObservers (NotifyFunc notify)
        {
            NotificationIterator iter(*this);
            
            while (BaseObserver *observer = iter.template beginNotify<RemoveNotified>()) {
                notify(*observer);
                iter.endNotify();
            }
        }
        
    private:
        class NotificationIterator
        {
        private:
            BaseObserver *m_observer;
            ListNode m_temp_node;
            
        public:
            inline NotificationIterator (BaseObservable &observable)
            {
                m_observer = static_cast<BaseObserver *>(observable.m_first);
            }
            
            template <bool RemoveNotified>
            BaseObserver * beginNotify ()
            {
                if (m_observer == nullptr) {
                    return nullptr;
                }
                
                m_temp_node.m_next = m_observer->m_next;
                
                if (RemoveNotified) {
                    m_temp_node.m_prev = m_observer->m_prev;
                    
                    m_observer->m_prev = nullptr;
                    
                    AIPSTACK_ASSERT(*m_temp_node.m_prev == m_observer);
                    *m_temp_node.m_prev = &m_temp_node;
                } else {
                    m_temp_node.m_prev = &m_observer->m_next;
                    
                    m_observer->m_next = &m_temp_node;
                }
                
                if (m_temp_node.m_next != nullptr) {
                    AIPSTACK_ASSERT(m_temp_node.m_next->m_prev == &m_observer->m_next);
                    m_temp_node.m_next->m_prev = &m_temp_node.m_next;
                }
                
                return m_observer;
            }
            
            void endNotify ()
            {
                // It is possible that reset was called while notifying.
                // In that case m_temp_node.m_prev was set to null. We have to check
                // this otherwise very bad things would happen.
                
                if (m_temp_node.m_prev == nullptr) {
                    m_observer = nullptr;
                    return;
                }
                
                m_observer = static_cast<BaseObserver *>(m_temp_node.m_next);
                
                remove_node(m_temp_node);
            }
        };
        
        inline void prepend_node (ListNode &node)
        {
            node.m_prev = &m_first;
            node.m_next = m_first;
            if (node.m_next != nullptr) {
                AIPSTACK_ASSERT(node.m_next->m_prev == &m_first);
                node.m_next->m_prev = &node.m_next;
            }
            m_first = &node;
        }
        
        inline static void remove_node (ListNode &node)
        {
            AIPSTACK_ASSERT(*node.m_prev == &node);
            *node.m_prev = node.m_next;
            if (node.m_next != nullptr) {
                AIPSTACK_ASSERT(node.m_next->m_prev == &node.m_next);
                node.m_next->m_prev = node.m_prev;
            }
        }
    };

    class BaseObserver :
        private ListNode,
        private NonCopyable<BaseObserver>
    {
        friend BaseObservable;
        
    public:
        inline BaseObserver ()
        {
            m_prev = nullptr;
        }
        
        inline ~BaseObserver ()
        {
            reset();
        }
        
        inline bool isActive () const
        {
            return m_prev != nullptr;
        }
        
        void reset ()
        {
            if (m_prev != nullptr) {
                BaseObservable::remove_node(*this);
                m_prev = nullptr;
            }
        }
        
    protected:
        void observeBase (BaseObservable &observable)
        {
            AIPSTACK_ASSERT(!isActive());
            
            observable.prepend_node(*this);
        }
    };
};

#endif

template <typename ObserverDerived>
class Observable;

/**
 * Represents an entity which can be associated with an observable and receive
 * notifications from it.
 * 
 * An observer is either unassociated or associated with a specific @ref Observable;
 * the initial state is unassociated. Association is established using
 * @ref Observable::addObserver.
 * 
 * The @ref Observer class does not define any specific method for passing notifications
 * from the observable to its observers. Instead, the @ref Observable::notifyKeepObservers
 * and @ref Observable::notifyRemoveObservers functions accept a callback function which is
 * called for each observer to notify it. This allows using an appropriate notification
 * mechanism for each use case.
 * 
 * One will generally define a class derived from @ref Observer when applying this
 * mechanism to a specific use case. Given this, possible approaches for polymorphic
 * notification are:
 * - Using a pure virtual function defined in the derived class.
 * - Using a @ref Function<Ret(Args...)> "Function" object passed to the constructor of
 *   the derived class.
 * 
 * The @ref Observer class must always be used as a base class. This is typically desired
 * anyway, and making the @ref Observer and @ref Observable aware of the derived observer
 * type allows the @ref Observable::notifyKeepObservers and
 * @ref Observable::notifyRemoveObservers functions to call the provided callback with the
 * derived observer type, so that the user does not need to perform the cast.
 * 
 * @tparam ObserverDerived Derived class type; each instance of
 *         @ref Observer<ObserverDerived> must be a base subobject of an `ObserverDerived`
 *         object.
 */
template <typename ObserverDerived>
class Observer
#ifndef IN_DOXYGEN
    :private ObserverNotificationPrivate::BaseObserver
#endif
{
    friend Observable<ObserverDerived>;
    
public:
    /**
     * Construct an unassociated observer.
     */
    inline Observer () = default;
    
    /**
     * Destruct the observer, disassociating it if associated.
     */
    inline ~Observer () = default;
    
    /**
     * Return whether the observer is associated.
     * 
     * @return True if associated, false if unassociated.
     */
    inline bool isActive () const
    {
        return BaseObserver::isActive();
    }
    
    /**
     * Disassociate the observer if associated.
     */
    inline void reset ()
    {
        return BaseObserver::reset();
    }
};

/**
 * Represents an entity which can notify its associated observers.
 * 
 * An observable has an associated set of observers (@ref Observer instances).
 * Observers can be associated and disassociated dynamically.
 * 
 * Observers associated with an observable are notified by calling @ref notifyKeepObservers
 * or @ref notifyRemoveObservers. These functions take a function object which
 * is called for each observer, passed as a reference to `ObserverDerived`.
 * 
 * This facility does not provide any spefific mechanism for notifying observers
 * (see the justification in @ref Observer).
 * 
 * @tparam ObserverDerived Derived class type of observers. Only
 *         `Observer<ObserverDerived>` observers can be associated with
 *         `Observable<ObserverDerived>`.
 */
template <typename ObserverDerived>
class Observable
#ifndef IN_DOXYGEN
    :private ObserverNotificationPrivate::BaseObservable
#endif
{
    using BaseObserver = ObserverNotificationPrivate::BaseObserver;
    
public:
    /**
     * Construct an observable with no associated observers.
     */
    Observable () = default;
    
    /**
     * Destruct the observable, disassociating any observers.
     */
    ~Observable () = default;
    
    /**
     * Return if the observable has any associated observers.
     * 
     * @return True if there is at least one associated observer, false if none.
     */
    inline bool hasObservers () const
    {
        return BaseObservable::hasObservers();
    }
    
    /**
     * Disassociate any observers from this observable.
     * 
     * Any observers which were associated with this observable become unassociated.
     */
    inline void reset ()
    {
        BaseObservable::reset();
    }
    
    /**
     * Associate an observer with this observable.
     * 
     * The observer being associated must be unassociated.
     * 
     * @param observer The observer to associate (must be unassociated).
     */
    inline void addObserver (Observer<ObserverDerived> &observer)
    {
        observer.BaseObserver::observeBase(*this);
    }
    
    /**
     * Enumerate the observers associated with this observable.
     * 
     * This calls `enumerate(ObserverDerived &)` for each observer. The order of
     * enumeration is not specified.
     * 
     * The `enumerate` function must not associate/disassociate any observers with/from
     * this observable and must not destruct this observable.
     * 
     * @tparam EnumerateFunc Function object type. Must be copy-constructible.
     * @param enumerate Function object to call for each observer.
     */
    template <typename EnumerateFunc>
    inline void enumerateObservers (EnumerateFunc enumerate)
    {
        BaseObservable::enumerateObservers(convertObserverFunc(enumerate));
    }
    
    /**
     * Notify the observers associated with this observable without disassociating them.
     * 
     * This calls `notify(ObserverDerived &)` for each observer. The order of
     * notifications is not specified.
     * 
     * The `notify` function is permitted to associate/disassociate observers with/from
     * this observable; the implementation is specifically designed to be safe in this
     * respect. However, the `notify` function must not destruct this observable.
     * 
     * @tparam NotifyFunc Function object type. Must be copy-constructible.
     * @param notify Function object to call to notify a specific observer.
     */
    template <typename NotifyFunc>
    inline void notifyKeepObservers (NotifyFunc notify)
    {
        BaseObservable::template notifyObservers<false>(convertObserverFunc(notify));
    }
    
    /**
     * Notify the observers associated with this observable while disassociating them.
     * 
     * This calls `notify(ObserverDerived &)` for each observer, disassociating each
     * observer from the observable just before its notify call. The order of notifications
     * is not specified.
     * 
     * The `notify` function is permitted to associate/disassociate observers with/from
     * this observable; the implementation is specifically designed to be safe in this
     * respect. However, the `notify` function must not destruct this observable.
     * 
     * @tparam NotifyFunc Function object type. Must be copy-constructible.
     * @param notify Function object to call to notify a specific observer.
     */
    template <typename NotifyFunc>
    inline void notifyRemoveObservers (NotifyFunc notify)
    {
        BaseObservable::template notifyObservers<true>(convertObserverFunc(notify));
    }
    
private:
    template <typename Func>
    inline static auto convertObserverFunc (Func func)
    {
        return [=](BaseObserver &base_observer) {
            auto &observer = static_cast<Observer<ObserverDerived> &>(base_observer);
            return func(static_cast<ObserverDerived &>(observer));
        };
    }
};

/** @} */

}

#endif
