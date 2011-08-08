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


#include "local_config.h"

#include <libmary/util_common.h>

#include <libmary/file.h>


namespace M {

#if 0
IoResult
File::readFull (Memory const &mem,
		Size * const nread)
    mt_throw (IoException
              InternalException)
{
    Size bread = 0;
    IoResult res = IoResult::Normal;

    while (bread < mem.len()) {
	Size last_read;
	res = read (mem.region (bread, mem.len() - bread), &last_read);
	if (res != IoResult::Normal)
	    break;

	bread += last_read;
    }

    if (nread)
	*nread = bread;

    return res;
}

IoResult
File::writeFull (ConstMemory const &mem,
		 Size * const nwritten)
    mt_throw (IoException
              InternalException)
{
    return writeFull_common (this, mem, nwritten);
#if 0
    Size bwritten = 0;
    IoResult res = IoResult::Normal;

    while (bwritten < mem.len()) {
	Size last_written;
	res = write (mem.region (bwritten, mem.len() - bwritten), &last_written);
	if (res != IoResult::Normal)
	    break;

	bwritten += last_written;
    }

    if (nwritten)
	*nwritten = bwritten;

    return res;
#endif
}
#endif

}

