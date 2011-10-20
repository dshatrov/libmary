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


#ifndef __LIBMARY__REF__H__
#define __LIBMARY__REF__H__


#include <libmary/types_base.h>
#include <libmary/util_base.h>


namespace M {

template <class T> class RetRef;

// TODO Apply some template metaprogramming to choose between
// "static_cast <BasicReferenced*>" and "static_cast <Referenced*>".

// Note: don't do static_cast <T*> (ref). Implicit casts should do the necessary
// conversions.
//
// Type T should have two methods to be usable with Ref<>:
//     * libMary_ref();
//     * libMary_unref.
// We want Ref<> to be usable with both BasicReferenced and Referenced classes.
// To get that, we require specific method to be present. The methods are name
// so that it is unlikely that they'll be overriden inadvertently.
//
template <class T>
class Ref
{
    template <class C> friend class Ref;

private:
    // 'obj' is mutable for RetRef, which derives from class Ref.
    // We should be able to do the following:
    //
    //     template <class C>
    //     Ref (RetRef<C> const &ref)
    //         : obj (ref.obj)
    //     {
    //         ref.obj = NULL;
    //     }
    //
    // I presume that this has no negative impact on performance (which must be
    // a very naive assumption). Prove me wrong.
    //
    // Note that C++0x move semantics references (&&) should allow to avoid
    // using 'mutable' here.
    //
    // Note also that we could duplicate the functionality of Ref in RetRef
    // without inheritance, avoiding the need to use 'mutable' in class Ref.
    T mutable *obj;

    void do_ref (T* const ref)
    {
	if (obj != NULL)
	    /*static_cast <Referenced*>*/ (obj)->libMary_unref ();

	obj = ref;
	if (obj != NULL)
	    /*static_cast <Referenced*>*/ (obj)->libMary_ref ();
    }

public:
/* TODO Explain why this method is necessary.

    T* ptr () const
    {
	return obj;
    }
*/

/* TODO Can't we just say "ref == NULL"?
 *      Or simply "if (ref) {}"?

    bool isNull () const
    {
	return obj == NULL;
    }

    operator T* () const
    {
	return obj;
    }
*/

    template <class C>
    operator C* () const
    {
	// TODO Is this check necessary?
	//      It is here because I was afraid of incorrect casts.
	//      NULL pointer casts are treated specially by the standard, so this
	//      must be unnecessary.
	if (obj == NULL)
	    return NULL;

	return obj;
    }

    // This is necessary for implicit conversions (like conversions to bool).
    // "template <class C> operator C* ()" is not sufficient.
    operator T* () const
    {
	return obj;
    }

    T* operator -> () const
    {
	return obj;
    }

    T& operator * () const
    {
	return *obj;
    }

    template <class C>
    void setNoUnref (C * const ref)
    {
	obj = ref;
	if (ref)
	    /*static_cast <Referenced*>*/ (obj)->libMary_ref ();
    }

    template <class C>
    void setNoRef (C * const ref)
    {
	if (obj != NULL)
	    /*static_cast <Referenced*>*/ (obj)->libMary_unref ();

	obj = ref;
    }

    template <class C>
    Ref& operator = (Ref<C> const &ref)
    {
	do_ref (ref.obj);
	return *this;
    }

    // Note that template <class C> Ref& opreator = (Ref<C> const &ref) does not
    // cover default assignment operator.
    Ref& operator = (Ref const &ref)
    {
	if (this == &ref)
	    return *this;

	do_ref (ref.obj);
	return *this;
    }

    template <class C>
    Ref& operator = (C* const ref)
    {
	do_ref (ref);
	return *this;
    }

    // This is necessary for the following to work:
    //     Ref<X> ref;
    //     ref = NULL;
    Ref& operator = (T* const ref)
    {
	do_ref (ref);
	return *this;
    }

    template <class C>
    Ref& operator = (RetRef<C> const &ref)
    {
	if (obj != NULL)
	    /*static_cast <Referenced*>*/ (obj)->libMary_unref ();

	obj = ref.obj;
	ref.obj = NULL;
	return *this;
    }

    static Ref<T> createNoRef (T* const ref)
    {
	Ref<T> tmp_ref;
	tmp_ref.obj = ref;
	return tmp_ref;
    }

    template <class C>
    Ref (Ref<C> const &ref)
	: obj (ref.obj)
    {
	if (ref.obj != NULL)
	    /*static_cast <Referenced*>*/ (ref.obj)->libMary_ref ();
    }

    // Note that template <class C> Ref (Ref<C> const &ref) does not cover
    // default copy constructor.
    //
    // We presume that it is impossible to pass a reference to self to a copy
    // constructor.
    Ref (Ref const &ref)
	: obj (ref.obj)
    {
	if (ref.obj != NULL)
	    /*static_cast <Referenced*>*/ (ref.obj)->libMary_ref ();
    }

    template <class C>
    Ref (C* const ref)
	: obj (ref)
    {
	if (ref != NULL)
	    /*static_cast <Referenced*>*/ (ref)->libMary_ref ();
    }

    // This is necessary for the following to work:
    //     Ref<X> ref (NULL);
    Ref (T * const ref)
	: obj (ref)
    {
	if (ref != NULL)
	    /*static_cast <Referenced*>*/ (ref)->libMary_ref ();
    }

    template <class C>
    Ref (RetRef<C> const &ref)
	: obj (ref.obj)
    {
	ref.obj = NULL;
    }

    Ref ()
	: obj (NULL)
    {
    }

    ~Ref ()
    {
	if (obj != NULL)
	    /*static_cast <Referenced*>*/ (obj)->libMary_unref ();
    }

    // MyCpp compatibility method.
    T* ptr () const
    {
	return obj;
    }

    T& der () const
    {
	assert (obj);
	return *obj;
    }

    bool isNull () const
    {
	return obj == NULL;
    }
};

// "Return value reference."
//
// Such references should be used only for function return values. Using them
// allows to avoid one excessive ref/unref pair when assigning retur values
// to references.
template <class T>
class RetRef : public Ref<T>
{
};

// "Grabbing" is needed because object's reference count is initiallized to '1'
// on object creation, which allows to use references freely in constructors.
template <class T>
Ref<T> grab (T * const obj)
{
    assert_hard (obj);
    return Ref<T>::createNoRef (obj);
}

}


#endif /* __LIBMARY__REF__H__ */

