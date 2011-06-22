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


#ifndef __LIBMARY__WEAK_REF__H__
#define __LIBMARY__WEAK_REF__H__


//#include <cstdio> // DEBUG


#include <libmary/ref.h>
#include <libmary/object.h>


namespace M {

//class Object;

// Note that WeakRef<Type> declarations should be valid for incomplete types.
//
template <class T>
class WeakRef
{
private:
//    Ref<typename T::_Shadow> shadow;
    Ref<Object::_Shadow> shadow;

    // 'typed_weak_ptr' has been introduced for compatibilty with MyCpp, which
    // uses virtual base classes heavily, like ": public virtual Object".
    //
    // This pointer is valid only when shadow->weak_ptr is non-NULL.
    // It is used to avoid dynamic_cast'ing shadow->weak_ptr, which is of type
    // 'Object*' in order to serve WeakRef<T> with any T.
    T *typed_weak_ptr;

public:
    Ref<T> getRef () const
    {
	if (!shadow)
	    return NULL;

//	Object * const obj = T::_GetRef (shadow);
	Object * const obj = Object::_GetRef (shadow);
	if (!obj)
	    return NULL;

	return Ref<T>::createNoRef (typed_weak_ptr);
    }

    T* getRefPtr () const
    {
	if (!shadow)
	    return NULL;

//	Object * const obj = T::_GetRef (shadow);
	Object * const obj = Object::_GetRef (shadow);
	if (!obj)
	    return NULL;

	return obj;
    }

    T* getTypedWeakPtr () const
    {
	return typed_weak_ptr;
    }

    bool isValid () const
    {
	return shadow;
    }

    WeakRef& operator = (T * const obj)
    {
	if (obj)
	    shadow = obj->getShadow ();
	else
	    shadow = NULL;

	typed_weak_ptr = obj;

	return *this;
    }

    WeakRef (T * const obj)
	: shadow (obj ? obj->getShadow () : NULL),
	  typed_weak_ptr (obj)
    {
    }

    WeakRef ()
      // There's no need to initialize typed_weak_ptr.
    {
    }

#if 0
    // DEBUG
    ~WeakRef ()
    {
	if (shadow)
	    fprintf (stderr, "--- ~WeakRef(): %lu\n", (unsigned long) shadow->getRefCount());
    }
#endif
};

}


#endif /* __LIBMARY__WEAK_REF__H__ */

