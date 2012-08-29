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


#ifndef __LIBMARY__VIRT_REF__H__
#define __LIBMARY__VIRT_REF__H__


#include <libmary/virt_referenced.h>


namespace M {

// This class is deliberately minimalistic. It is meant to be used in Callback
// only to hold a referenced to an arbitrary object (Referenced or Object).
class VirtRef
{
private:
    VirtReferenced *ref;

public:
    VirtReferenced* ptr () const
    {
	return ref;
    }

#if 0
    void setNoUnref (VirtReferenced * const ref)
    {
	this->ref = ref;
	if (ref)
	    ref->virt_ref ();
    }
#endif

    // TODO Unrefing late in all *Ref<> classes is probably a good idea.
    //      ^^^ For sure!
    void selfUnref ()
    {
        if (this->ref) {
            VirtReferenced * const tmp_ref = this->ref;
            this->ref = NULL;
            tmp_ref->virt_unref ();
        }
    }

    VirtRef& operator = (VirtReferenced * const ref)
    {
        if (this->ref == ref)
            return *this;

	if (this->ref)
	    this->ref->virt_unref ();

	this->ref = ref;
	if (ref)
	    ref->virt_ref ();

	return *this;
    }

    VirtRef& operator = (VirtRef const &virt_ref)
    {
	if (this == &virt_ref || ref == virt_ref.ref)
	    return *this;

	if (ref)
	    ref->virt_unref ();

	ref = virt_ref.ref;
	if (ref)
	    ref->virt_ref ();

	return *this;
    }

    VirtRef (VirtRef const &virt_ref)
        : ref (virt_ref.ref)
    {
	if (ref)
	    ref->virt_ref ();
    }

    VirtRef (VirtReferenced * const ref)
	: ref (ref)
    {
	if (ref)
	    ref->virt_ref ();
    }

    VirtRef ()
	: ref (NULL)
    {
    }

    ~VirtRef ()
    {
	if (ref)
	    ref->virt_unref ();
    }
};

}


#endif /* __LIBMARY__VIRT_REF__H__ */

