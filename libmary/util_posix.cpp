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

#include <libmary/types.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <libmary/log.h>

#include <libmary/util_posix.h>


namespace M {

mt_throws Result posix_createNonblockingPipe (int (*fd) [2])
{
    {
	int const res = pipe (*fd);
	if (res == -1) {
	    exc_throw <PosixException> (errno);
	    exc_push <InternalException> (InternalException::BackendError);
	    logE_ (_func, "pipe() failed: ", errnoString (errno));
	    return Result::Failure;
	} else
	if (res != 0) {
	    exc_throw <InternalException> (InternalException::BackendMalfunction);
	    logE_ (_func, "pipe(): unexpected return value: ", res);
	    return Result::Failure;
	}
    }

    for (int i = 0; i < 2; ++i) {
	int flags = fcntl ((*fd) [i], F_GETFL, 0);
	if (flags == -1) {
	    exc_throw <PosixException> (errno);
	    exc_push <InternalException> (InternalException::BackendError);
	    logE_ (_func, "fcntl() failed (fd[", i, "]): ", errnoString (errno));
	    return Result::Failure;
	}

	flags |= O_NONBLOCK;

	if (fcntl ((*fd) [i], F_SETFL, flags) == -1) {
	    exc_throw <PosixException> (errno);
	    exc_push <InternalException> (InternalException::BackendError);
	    logE_ (_func, "fcntl() failed (F_SETFL, fd[", i, "]): ", errnoString (errno));
	    return Result::Failure;
	}
    }

    return Result::Success;
}

}

