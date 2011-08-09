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


#ifndef __LIBMARY__ATOMIC__H__
#define __LIBMARY__ATOMIC__H__


#include <glib/gatomic.h>


namespace M {

class AtomicInt
{
private:
    volatile gint value;

public:
    void set (int const value)
    {
	g_atomic_int_set (&this->value, (gint) value);
    }

    int get () const
    {
	return (int) g_atomic_int_get (&value);
    }

    void inc ()
    {
	g_atomic_int_inc (&value);
    }

    void add (int const a)
    {
	g_atomic_int_add (&value, (gint) a);
    }

    int fetchAdd (int const a)
    {
	return (int) g_atomic_int_exchange_and_add (&value, a);

#if 0
// Alternate version.
	for (;;) {
	    gint const old = g_atomic_int_get (&value);
	    if (g_atomic_int_compare_and_exchange (&value, old, old + a))
		return old;
	}
	// unreachable
#endif
    }

    bool compareAndExchange (int const old_value,
			     int const new_value)
    {
	return (bool) g_atomic_int_compare_and_exchange (
			      &value, (gint) old_value, (gint) new_value);
    }

    bool decAndTest ()
    {
	return (bool) g_atomic_int_dec_and_test (&value);
    }

    AtomicInt (int const value = 0)
    {
	g_atomic_int_set (&this->value, (gint) value);
    }
};

class AtomicPointer
{
private:
    volatile gpointer value;

public:
    void set (void * const value)
    {
	g_atomic_pointer_set (&this->value, (gpointer) value);
    }

    void* get () const
    {
	return (void*) g_atomic_pointer_get (&this->value);
    }

    // _nonatomic methods have been added to reuse Object::shadow for
    // deletion queue list link pointer.
    void set_nonatomic (void * const value)
    {
	this->value = (gpointer) value;
    }

    void* get_nonatomic () const
    {
	return (void*) value;
    }

    bool compareAndExchange (void * const old_value,
			     void * const new_value)
    {
	return (bool) g_atomic_pointer_compare_and_exchange (&value,
							     (gpointer) old_value,
							     (gpointer) new_value);
    }

    AtomicPointer (void * const value = NULL)
    {
	g_atomic_pointer_set (&this->value, value);
    }
};

}


#endif /* __LIBMARY__ATOMIC__H__ */

