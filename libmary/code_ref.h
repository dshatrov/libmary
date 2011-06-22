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


#ifndef __LIBMARY__CODE_REF__H__
#define __LIBMARY__CODE_REF__H__


#include <libmary/types_base.h>
#include <libmary/code_referenced.h>
#include <libmary/object.h>
#include <libmary/ref.h>
#include <libmary/weak_ref.h>
#include <libmary/libmary_thread_local.h>


#ifdef DEBUG
#error DEBUG already defined
#endif
#define DEBUG(a)
#if DEBUG(1) + 0
#include <cstdio>
#endif


namespace M {

class CodeRef;

class WeakCodeRef
{
    friend class CodeRef;

private:
    // We store container pointer to optimize away consequent reference grabs
    // for the same container in runtime. That decreases number of atomic ops
    // performed when processing async events with chains of handler objects.
    Object *weak_obj;
    // This is Ref<Object::_Shadow>, actually.
    // TODO Зачем здесь вообще weak_ref? Поскольку корректность работы теперь
    //      зависит от простого указателя weak_obj, никакого смысла в weak_ref
    //      нет. Можно просто делать weak_obj->ref()/unref().
    //      ^^^ 11.05.31 Не согласен с замечанием, без weak_ref не будет
    //          безопасного захвата CodeRef.
    WeakRef<Object> weak_ref;

public:
    bool isValid () const
    {
	return weak_ref.isValid ();
    }

    Object* getWeakObject () const
    {
	return weak_obj;
    }

    WeakCodeRef& operator = (CodeReferenced * const obj)
    {
	Object * const container = obj ? obj->getCoderefContainer() : NULL;
	weak_ref = container;
	weak_obj = container;
	return *this;
    }

    WeakCodeRef (CodeReferenced * const obj)
	: weak_obj (obj ? obj->getCoderefContainer() : NULL),
	  // We had to trick with initialization order here to avoid calling
	  // obj->getCoderefContainer() twice.
	  weak_ref (weak_obj)
    {
    }

    WeakCodeRef ()
	: weak_obj (NULL)
    {
    }
};

// TODO Ref<X> ref = weak_ref;
//      if (ref) { ... }
//      ^^^ Такая запись лучше с т з оптимизации, чем "Ref<X> ref = weak_ref.getRef();"
//          Это особенно важно потому, что операции с AtomicInt дороги.
//
class CodeRef
{
private:
//    CodeReferenced * const obj;
    Ref<Object> ref;

public:
    operator Object* () const
    {
	return ref;
    }

    Object* operator -> () const
    {
	return ref;
    }

    CodeRef& operator = (CodeReferenced * const obj)
    {
	this->ref = obj ? obj->getCoderefContainer() : NULL;
	return *this;
    }

    CodeRef (WeakCodeRef const &weak_ref)
	: ref (weak_ref.weak_ref.getRef ())
    {
	DEBUG (
	  fprintf (stderr, "CodeRef(WeakCodeRef const &): obj 0x%lx\n", (unsigned long) (void*) ref);
	)
    }

    CodeRef (CodeReferenced * const obj)
	: ref (obj ? obj->getCoderefContainer() : NULL)
    {
	DEBUG (
	  fprintf (stderr, "CodeRef(CodeReferenced*): obj 0x%lx\n", (unsigned long) (void*) ref);
	)
    }

    CodeRef ()
    {
	DEBUG (
	  fprintf (stderr, "CodeRef()\n");
	)
    }

  DEBUG (
    ~CodeRef ()
    {
	fprintf (stderr, "~CodeRef: obj 0x%lx\n", (unsigned long) (void*) ref);
    }
  )
};

// TODO Refine this (see docs/informer.txt).
template <class T, class CB, class ...Args>
void
CodeReferenced::async_call (T *self, CB tocall, Args const &...args)
{
    if (!self || !tocall)
	return false;

    Object* const coderef_container = getCoderefContainer();

    // TODO Почему бы не делать эти действия в CodeRef?
    //      Касается также класса Cb<>.
    //      Если бы за проверку tlocal отвечал CodeRef, то async_call
    //      выродился бы до:
    //          {
    //              CodeRef ref = obj;
    //              obj->method ();
    //          }
    //      И потребность в async_call() пропала бы полностью.
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    if (coderef_container == tlocal->last_coderef_container) {
	self->*tocall (args...);
	return;
    }

    CodeRef const code_ref = coderef_container;
    Object * const prv_coderef_container = tlocal->last_coderef_container;
    tlocal->last_coderef_container = coderef_container;

    self->*tocall (args...);

    tlocal->last_coderef_container = prv_coderef_container;
}

}


#ifdef DEBUG
#undef DEBUG
#endif


#endif /* __LIBMARY__CODE_REF__H__ */

