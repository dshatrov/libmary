/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2012 Dmitry Shatrov

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


#ifndef __LIBMARY__DEP_REF__H__
#define __LIBMARY__DEP_REF__H__


#include <libmary/code_referenced.h>
#include <libmary/weak_ref.h>


namespace M {

template <class T> class DepRef;

template <class T>
class WeakDepRef
{
    template <class C> friend class DepRef;

private:
    T *unsafe_obj;
    Object *weak_obj;
    WeakRef<Object> weak_ref;

public:
    bool isValid () const
    {
        return weak_ref.isValid();
    }

    Object* getWeakObject () const
    {
        return weak_obj;
    }

    T* getUnsafePtr () const
    {
        return unsafe_obj;
    }

    WeakDepRef& operator = (T * const obj)
    {
        Object * const container = obj ? obj->getCoderefContainer() : NULL;
        unsafe_obj = obj;
        weak_ref = container;
        weak_obj = container;
        return *this;
    }

    WeakDepRef (CodeReferenced * const obj)
        : unsafe_obj (obj),
          weak_obj (obj ? obj->getCoderefContainer() : NULL),
          weak_ref (weak_obj)
    {
    }

    WeakDepRef ()
        : unsafe_obj (NULL),
          weak_obj (NULL)
    {
    }
};

template <class T>
class DepRef
{
private:
    T *obj_ptr;
    Ref<Object> ref;

public:
    operator T* () const
    {
        return obj_ptr;
    }

    T* operator -> () const
    {
        return obj_ptr;
    }

    DepRef& operator = (DepRef * const obj)
    {
        this->obj_ptr = obj->obj_ptr;
        this->ref = obj ? obj->getCoderefContainer() : NULL;
        return *this;
    }

    DepRef (WeakDepRef<T> const &weak_ref)
        : obj_ptr (weak_ref.unsafe_obj),
          ref (weak_ref.weak_ref.getRef())
    {
    }

    DepRef (T * const obj)
        : obj_ptr (obj),
          ref (obj ? obj->getCoderefContainer() : NULL)
    {
    }

    DepRef ()
        : obj_ptr (NULL)
    {
    }
};

}


#endif /* __LIBMARY__DEP_REF__H__ */

