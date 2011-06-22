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


#ifndef __LIBMARY__ASYNC_INPUT_STREAM__H__
#define __LIBMARY__ASYNC_INPUT_STREAM__H__


#include <libmary/types.h>
#include <libmary/exception.h>


namespace M {

class AsyncInputStream
{
public:
    // TODO ret_again to tell that the next call to read() will yield AsyncIoResult::Again.
    // ret_flags may be a good match.
    virtual mt_throws AsyncIoResult read (Memory const &mem,
					  Size *ret_nread,
					  bool *ret_eof = NULL) = 0;
};

}


#endif /* __LIBMARY__ASYNC_INPUT_STREAM__H__ */

