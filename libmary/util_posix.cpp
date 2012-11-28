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
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <libmary/log.h>
#include <libmary/util_dev.h>

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

mt_throws Result commonTriggerPipeWrite (int const fd)
{
    for (;;) {
	ssize_t const res = write (fd, "A", 1);
	if (res == -1) {
	    if (errno == EINTR)
		continue;

	    if (errno == EAGAIN || errno == EWOULDBLOCK)
		break;

	    exc_throw <PosixException> (errno);
	    exc_push <InternalException> (InternalException::BackendError);
	    logE_ (_func, "write() failed: ", errnoString (errno));
	    return Result::Failure;
	} else
	if (res != 1 && res != 0) {
	    exc_throw <InternalException> (InternalException::BackendMalfunction);
	    logE_ (_func, "write(): unexpected return value: ", res);
	    return Result::Failure;
	}

	// If res is 0, then we don't care, because this means that the pipe is
	// full of unread data, and the poll group will be triggered by that
	// data	anyway.

	break;
    }

    return Result::Success;
}

mt_throws Result commonTriggerPipeRead (int const fd)
{
    for (;;) {
	Byte buf [128];
	ssize_t const res = read (fd, buf, sizeof (buf));
	if (res == -1) {
	    if (errno == EINTR)
		continue;

	    if (errno == EAGAIN || errno == EWOULDBLOCK)
		break;

	    exc_throw <PosixException> (errno);
	    exc_push <InternalException> (InternalException::BackendError);
	    logE_ (_func, "read() failed (trigger pipe): ", errnoString (errno));
	    return Result::Failure;
	} else
	if (res < 0 || (Size) res > sizeof (buf)) {
	    exc_throw <InternalException> (InternalException::BackendMalfunction);
	    logE_ (_func, "read(): unexpected return value (trigger pipe): ", res);
	    return Result::Failure;
	}

	if ((Size) res < sizeof (buf)) {
	  // Optimizing away an extra read() syscall.
	    break;
	}
    }

    return Result::Success;
}

mt_throws Result posix_statToFileStat (struct stat * const mt_nonnull stat_buf,
                                       FileStat    * const mt_nonnull ret_stat)
{
    if (S_ISBLK (stat_buf->st_mode))
	ret_stat->file_type = FileType::BlockDevice;
    else
    if (S_ISCHR (stat_buf->st_mode))
	ret_stat->file_type = FileType::CharacterDevice;
    else
    if (S_ISDIR (stat_buf->st_mode))
	ret_stat->file_type = FileType::Directory;
    else
    if (S_ISFIFO (stat_buf->st_mode))
	ret_stat->file_type = FileType::Fifo;
    else
    if (S_ISREG (stat_buf->st_mode))
	ret_stat->file_type = FileType::RegularFile;
#ifndef LIBMARY_PLATFORM_WIN32
    else
    if (S_ISLNK (stat_buf->st_mode))
	ret_stat->file_type = FileType::SymbolicLink;
    else
    if (S_ISSOCK (stat_buf->st_mode))
	ret_stat->file_type = FileType::Socket;
#endif
    else {
	logE_ (_func, "Unknown file type:");
        logLock ();
	hexdump (logs, ConstMemory::forObject (stat_buf->st_mode));
        logUnlock ();
	exc_throw <InternalException> (InternalException::BackendMalfunction);
	return Result::Failure;
    }

    ret_stat->size = (unsigned long long) stat_buf->st_size;

    return Result::Success;
}

}

