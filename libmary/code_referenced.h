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


#ifndef __LIBMARY__CODE_REFERENCED__H__
#define __LIBMARY__CODE_REFERENCED__H__


#include <libmary/annotations.h>


namespace M {

class Object;

class CodeReferenced
{
public:
    virtual Object* getCoderefContainer () = 0;

    // Implemented in code_ref.h to resolve header dependencies.
    template <class T, class CB, class ...Args>
    void async_call (T *self, CB tocall, Args const &...args);

    virtual ~CodeReferenced () {}
};

class DependentCodeReferenced : public virtual CodeReferenced
{
private:
    mt_const Object *coderef_container;

public:
    Object* getCoderefContainer ()
    {
	return coderef_container;
    }

    DependentCodeReferenced (Object * const coderef_container)
	: coderef_container (coderef_container)
    {
    }
};

}


#include <libmary/object.h>


#endif /* __LIBMARY__CODE_REFERENCED__H__ */

