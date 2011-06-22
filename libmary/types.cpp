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


#include <libmary/types.h>


namespace M {

Size
AsyncIoResult::toString_ (Memory const &mem,
			  Format const &fmt)
{
    switch (value) {
	case Normal:
	    return toString (mem, "AsyncIoResult::Normal");
	case Again:
	    return toString (mem, "AsyncIoResult::Again");
	case Eof:
	    return toString (mem, "AsyncIoResult::Eof");
	case Error:
	    return toString (mem, "AsyncIoResult::Error");
    }

    unreachable ();
    return 0;
}

}

