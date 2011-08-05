/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2011 Dmitry Shatrov

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/


#ifndef __LIBMARY__OBJECT__H__
#define __LIBMARY__OBJECT__H__


#include <libmary/intrusive_list.h>
#include <libmary/atomic.h>
#include <libmary/state_mutex.h>
#include <libmary/referenced.h>
#include <libmary/ref.h>
#include <libmary/code_referenced.h>


#ifdef DEBUG
#error DEBUG already defined
#endif
#define DEBUG(a)
#if DEBUG(1) + 0
#include <cstdio>
#endif


namespace M {

class Object;

void deletionQueue_append (Object * const obj);
void deletionQueue_process ();

template <class T> class Callback;

// Object inherits from Referenced for compatibility with MyCpp. Virtual
// inheritance is here for the same reason. It shouldn't be too much of
// overhead, though.
class Object : public virtual Referenced,
	       public CodeReferenced
{
    template <class T> friend class WeakRef;

    friend void deletionQueue_append (Object * const obj);
    friend void deletionQueue_process ();

public:
    typedef void (*DeletionCallback) (void *data);

    class DeletionSubscription;

    // This wrapper has been introduced to ease migrating MyCpp to LibMary
    // refcounting code. Originally, DeletionSubscriptionKey was a typedef for
    // DeletionSubscription*.
    class DeletionSubscriptionKey
    {
	friend class Object;
    private:
	DeletionSubscription *del_sbn;
	DeletionSubscription* operator -> () const { return del_sbn; }
    public:
	bool isNull () const { return del_sbn == NULL; } // Eases transition to M::Object for MyCpp.
	// TODO This makes the wrapper useless. Replace this with operator bool ().
	operator DeletionSubscription* () const { return del_sbn; }
	// TODO Only class Object should be able to create bound deletion keys.
	DeletionSubscriptionKey (DeletionSubscription * const del_sbn) : del_sbn (del_sbn) {}
	DeletionSubscriptionKey () : del_sbn (NULL) {}
    };

// protected:
public:
    // Beware that size of pthread_mutex_t is 24 bytes on 32-bit platforms and
    // 40 bytes on 64-bit ones. We definitely do not want more than one mutex
    // per object, which is already too much overhead.
    StateMutex mutex;

private:
  // Class WeakRef may access the following private members as a friend.

    class _Shadow : public Referenced
    {
	friend class Object;
	template <class T> friend class WeakRef;

    private:
	Mutex mutex;
	Object *weak_ptr;

	// This counter ensures that the object will be deleted sanely when
	// series of _GetRef()/unref() calls sneak in while last_unref() is
	// in progress. In this case, we'll have multiple invocations of
	// last_unref(), and we should be able to determine which of those
	// invocations is the last one.
	Count lastref_cnt;

	void lock ()
	{
	    mutex.lock ();
	}

	void unlock ()
	{
	    mutex.unlock ();
	}

	DEBUG (
	    _Shadow ()
	    {
		static char const * const _func_name = "LibMary.Object._Shadow()";
		printf ("0x%lx %s\n", (unsigned long) this, _func_name);
	    }

	    ~_Shadow ()
	    {
		static char const * const _func_name = "LibMary.Object.~_Shadow()";
		printf ("0x%lx %s\n", (unsigned long) this, _func_name);
	    }
	)
    };

    // There's no need to return Ref<_Shadow> here:
    //   * We're supposed to have a valid reference to the object for duration
    //     of getShadow() call;
    //   * Once bound to the object, the shadow's life time is not shorter than
    //     object's lifetime.
    _Shadow* getShadow ()
    {
	_Shadow *shadow = static_cast <_Shadow*> (atomic_shadow.get ());
	if (shadow)
	    return shadow;

	// TODO Slab cache for Object_Shadow objects: describe the idea.

	// Shadow stays referenced until it is unrefed in ~Object().
	shadow = new _Shadow ();
	shadow->weak_ptr = this;
	shadow->lastref_cnt = 1;

	if (atomic_shadow.compareAndExchange (NULL, static_cast <void*> (shadow)))
	    return shadow;

	// We assume that high contention on getShadow() is unlikely, hence
	// occasional deletes do not bring much overhead.
	delete shadow;

	return static_cast <_Shadow*> (atomic_shadow.get ());
    }

    // _GetRef() is specific to WeakRef::getRef(). It is a more complex subcase
    // of ref().
    static Object* _GetRef (_Shadow * const mt_nonnull shadow)
    {
      MutexLock shadow_l (&shadow->mutex);

	Object * const obj = shadow->weak_ptr;

	DEBUG (
	  fprintf (stderr, "Object::_GetRef: shadow 0x%lx, obj 0x%lx\n", (unsigned long) shadow, (unsigned long) obj);
	)

        if (!obj) {
	    return NULL;
	}

	if (obj->refcount.fetchAdd (1) == 0)
	    ++shadow->lastref_cnt;

	return obj;
    }

  // (End of what class WeakRef may access.)

    AtomicPointer atomic_shadow;

    IntrusiveCircularList<DeletionSubscription> deletion_subscription_list;

    static void mutualDeletionCallback (void * mt_nonnull _sbn);

    virtual void last_unref ();

    // May be called directly by deletionQueue_process().
    void do_delete ();

    // We forbid copying until it is shown that it might be convenient in some cases.
    Object& operator = (Object const &);
    Object (Object const &);

public:
  mt_iface (CodeReferenced)

      Object* getCoderefContainer ()
      {
	  return this;
      }

  mt_iface_end (CodeReferenced)

    mt_locked DeletionSubscriptionKey addDeletionCallback (DeletionCallback  cb,
							   void             *cb_data,
							   Referenced       *ref_data,
							   Object           *guard_obj);

    DeletionSubscriptionKey addDeletionCallback_unlocked (DeletionCallback  cb,
							  void             *cb_data,
							  Referenced       *ref_data,
							  Object           *guard_obj);

    mt_locked DeletionSubscriptionKey addDeletionCallback_mutualUnlocked (DeletionCallback  cb,
									  void             *cb_data,
									  Referenced       *ref_data,
									  Object           *guard_obj);

    // TODO Is it really necessary to create a new DeletionCallback object for
    // mutual deletion callbacks? Perhaps the existing DeletionCallback object
    // could be reused.
    mt_locked DeletionSubscriptionKey addDeletionCallbackNonmutual (DeletionCallback  cb,
								    void             *cb_data,
								    Referenced       *ref_data,
								    Object           *guard_obj);

    mt_locked DeletionSubscriptionKey addDeletionCallbackNonmutual_unlocked (DeletionCallback  cb,
									     void             *cb_data,
									     Referenced       *ref_data,
									     Object           *guard_obj);

    void removeDeletionCallback (DeletionSubscriptionKey mt_nonnull sbn);

    void removeDeletionCallback_unlocked (DeletionSubscriptionKey mt_nonnull sbn);

    // Should be called when when state mutex of the subscriber is locked.
    void removeDeletionCallback_mutualUnlocked (DeletionSubscriptionKey mt_nonnull sbn);

    Object ()
    {
    }

    virtual ~Object ();

#if 0
// MOVED TO CodeReferenced
    // TODO Refine this (see docs/informer.txt).
    template <class Cb, class ...Args>
    void async_call (CB tocall, Args const &...args)
    {
	if (!tocall)
	    return false;

	LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
	if (this == tlocal->last_coderef_container) {
	    tocall (args...);
	    return;
	}

	CodeRef const code_ref = this;
	Object * const prv_coderef_container = tlocal->last_coderef_container;
	tlocal->last_coderef_container = this;

	tocall (args...);

	tlocal->last_coderef_container = prv_coderef_container;
    }
#endif
};

template <class T>
class ObjectWrap : public Object, public T
{
};

}


#include <libmary/weak_ref.h>


namespace M {

// TODO DeletionSubscription and mutual deletion subscription can be stored
//      in a single area of memory, which would spare us one malloc() per
//      deletion subscription.
class Object::DeletionSubscription : public IntrusiveListElement<>
{
    friend class Object;

private:
    DeletionCallback   const cb;
    void             * const cb_data;
    Ref<Referenced>    const ref_data;
    WeakRef<Object>    const weak_peer_obj;

    // Subscription for deletion of the peer.
    DeletionSubscriptionKey mutual_sbn;

    // Pointer to self (for mutual deletion callback).
    Object *obj;

    DeletionSubscription (DeletionCallback   const cb,
			  void             * const cb_data,
			  Referenced       * const ref_data,
			  Object           * const guard_obj)
	: cb (cb),
	  cb_data (cb_data),
	  ref_data (ref_data),
	  weak_peer_obj (guard_obj)
    {
    }
};

inline Object::~Object ()
{
    DEBUG (
	static char const * const _func_name = "LibMary.Object.~Object()";
    )

    DEBUG (
	printf ("0x%lx %s\n", (unsigned long) this, _func_name);
    )

    {
      // Note: Here we count on that we can read atomic_shadow as an atomic
      // variable correctly even if we've been using it in non-atomic fashion
      // as deletion queue link pointer. In practice, this should work, but we'll
      // be able to count on this 100% only whith C++0x.

	_Shadow * const shadow = static_cast <_Shadow*> (atomic_shadow.get ());
	if (shadow)
	    shadow->unref ();
    }

  // deletion_subscription_list must be empty at this moment. We have released
  // all subscriptions in do_delete. We can only check that it holds here with
  // state mutex held, so we don't do that.

    DEBUG (
	printf ("0x%lx %s: done\n", (unsigned long) this, _func_name);
    )
}

}


#ifdef DEBUG
#undef DEBUG
#endif


#endif /* __LIBMARY__OBJECT__H__ */

