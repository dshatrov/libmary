/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2011, 2012 Dmitry Shatrov

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


#ifndef __LIBMARY__EXCEPTION_BUFFER__H__
#define __LIBMARY__EXCEPTION_BUFFER__H__


#include <libmary/types.h>
#include <cstdlib>

#include <libmary/referenced.h>


#define LIBMARY__EXCEPTION_BUFFER_SIZE 4096


namespace M {

class Exception;

class ExceptionBuffer : public Referenced
{
private:
    Byte *data_buf;
    Size data_len;
    Size alloc_len;

public:
    Exception* getException () const
    {
        return reinterpret_cast <Exception*> (data_buf);
    }

    Byte* throw_ (Size const len)
    {
	data_len = len;
	return data_buf;
    }

    // Returns NULL when the buffer is full.
    Byte* push (Size len);

    void reset ()
    {
	data_len = 0;
    }

    ExceptionBuffer (Size const alloc_len)
	: alloc_len (alloc_len)
    {
	data_buf = reinterpret_cast <Byte*> (malloc (alloc_len));
	assert_hard (data_buf);
	data_len = alloc_len;
    }

    ~ExceptionBuffer ()
    {
	free (data_buf);
    }
};

}


#endif /* __LIBMARY__EXCEPTION_BUFFER__H__ */

