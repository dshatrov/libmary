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


#ifndef __LIBMARY__CB__H__
#define __LIBMARY__CB__H__


#include <libmary/code_ref.h>
#include <libmary/libmary_thread_local.h>


#ifdef DEBUG
#error DEBUG already defined
#endif
#define DEBUG(a)
#if DEBUG(1) + 0
#include <cstdio>
#endif


namespace M {

// TODO class CbDesc for things like getPollable()

template <class T>
class Cb
{
private:
    // Frontend/backend.
    T const *cb;
    void *cb_data;
    WeakCodeRef weak_code_ref;

public:
    // Returns 'true' if the callback has actually been called.
    // Returns 'false' if we couldn't grab a code referenced for the callback.
    template <class RET, class CB, class ...Args>
    bool call_ret (RET * const mt_nonnull ret, CB tocall, Args const &...args) const
    {
	if (!tocall) {
	    DEBUG (
	      fprintf (stderr, "Cb::call_ret: callback not set, obj 0x%lx\n", (unsigned long) weak_code_ref.getWeakObject());
	    )
	    return false;
	}

	if (weak_code_ref.isValid ()) {
	  // The weak reference is valid, which means that we should grab a real
	  // code reference first.

	    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal ();
	    if (weak_code_ref.getWeakObject() == tlocal->last_coderef_container) {
	      // The container has already been code-referenced in the current
	      // execution path. There's no need to duplicate the reference.
	      //
	      // In this case, it must be possible to prove that we're not
	      // comparing a pointer to an old object to a pointer to a new
	      // object located at the same address. This is true for conatiner
	      // logics in MomentVideo.
		goto _simple_path;
	    }

	    CodeRef const code_ref = weak_code_ref;
	    if (!code_ref) {
		DEBUG (
		  fprintf (stderr, "Cb::call_ret: obj 0x%lx gone\n", (unsigned long) weak_code_ref.getWeakObject());
		)
		return false;
	    }
	    DEBUG (
	      fprintf (stderr, "Cb::call_ret: refed obj 0x%lx\n", (unsigned long) weak_code_ref.getWeakObject());
	    )

	    Object * const prv_coderef_container = tlocal->last_coderef_container;
	    tlocal->last_coderef_container = weak_code_ref.getWeakObject ();

	    // This probably won't be optimized by the compiler, which is a bit sad.
	    *ret = tocall (args..., cb_data);

	    tlocal->last_coderef_container = prv_coderef_container;
	    return true;
	} else {
	    DEBUG (
	      fprintf (stderr, "Cb::call_ret: no weak obj\n");
	    )
	}

      _simple_path:
	DEBUG (
	  fprintf (stderr, "Cb::call_ret: simple path, obj 0x%lx\n", (unsigned long) weak_code_ref.getWeakObject());
	)
	*ret = tocall (args..., cb_data);
	DEBUG (
	  fprintf (stderr, "Cb::call_ret: done, obj 0x%lx\n", (unsigned long) weak_code_ref.getWeakObject());
	)
	return true;
    }

    // Convenient method of invoking callbacks which return void.
    //
    // Returns 'true' if the callback has actually been called.
    // Returns 'false' if we couldn't grab a code referenced for the callback.
    template <class CB, class ...Args>
    bool call (CB tocall, Args const &...args) const
    {
	if (!tocall) {
	    DEBUG (
	      fprintf (stderr, "Cb::call: callback not set, obj 0x%lx\n", (unsigned long) weak_code_ref.getWeakObject());
	    )
	    return false;
	}

	if (weak_code_ref.isValid ()) {
	    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal ();
	    if (weak_code_ref.getWeakObject() == tlocal->last_coderef_container)
		goto _simple_path;

	    CodeRef const code_ref = weak_code_ref;
	    if (!code_ref) {
		DEBUG (
		  fprintf (stderr, "Cb::call: obj 0x%lx gone\n", (unsigned long) weak_code_ref.getWeakObject());
		)
		return false;
	    }
	    DEBUG (
	      fprintf (stderr, "Cb::call: refed obj 0x%lx\n", (unsigned long) weak_code_ref.getWeakObject());
	    )

	    Object * const prv_coderef_container = tlocal->last_coderef_container;
	    tlocal->last_coderef_container = weak_code_ref.getWeakObject ();

	    tocall (args..., cb_data);

	    tlocal->last_coderef_container = prv_coderef_container;
	    return true;
	} else {
	    DEBUG (
	      fprintf (stderr, "Cb::call: no weak obj\n");
	    )
	}

      _simple_path:
	DEBUG (
	  fprintf (stderr, "Cb::call: simple path, obj 0x%lx\n", (unsigned long) weak_code_ref.getWeakObject());
	)
	tocall (args..., cb_data);
	DEBUG (
	  fprintf (stderr, "Cb::call: done, obj 0x%lx\n", (unsigned long) weak_code_ref.getWeakObject());
	)
	return true;
    }

    template <class RET, class ...Args>
    bool call_ret_ (RET * const mt_nonnull ret, Args const &...args) const
    {
	return call_ret (ret, cb, args...);
    }

    template <class ...Args>
    bool call_ (Args const &...args) const
    {
	return call (cb, args...);
    }

#if 0
// Commented out for safe transition to Cb::call()/Cb::call_ret().
    void* data () const
    {
	return cb_data;
    }
#endif

    WeakCodeRef const & getWeakCodeRef () const
    {
	return weak_code_ref;
    }

    // For debugging only.
    Object* getWeakObject () const
    {
	return weak_code_ref.getWeakObject ();
    }

    void* getCbData () const
    {
	return cb_data;
    }

    operator T const * () const
    {
	return cb;
    }

    T const * operator -> () const
    {
	return cb;
    }

    // TODO After introduction of coderef containers, copying Cb<>'s around
    //      became more expensive because of Ref<_Shadow> member. Figure out
    //      what's the most effective way to avoid excessive atomic ref/unref
    //      operations.
    Cb (T const * const cb,
	void    * const cb_data,
	Object  * const coderef_container)
	: cb (cb),
	  cb_data (cb_data),
	  weak_code_ref (coderef_container)
    {
    }

    Cb ()
	: cb (NULL),
	  cb_data (NULL)
    {
    }
};

}


#ifdef DEBUG
#undef DEBUG
#endif


#endif /* __LIBMARY__CB__H__ */

