/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2011-2013 Dmitry Shatrov

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


#include <libmary/file_connection.h>


namespace M {

mt_throws AsyncIoResult
FileConnection::read (
#ifdef LIBMARY_WIN32_IOCP
                      OVERLAPPED  * const mt_nonnull /* overlapped */,
#endif
                      Memory        const mem,
		      Size        * const ret_nread)
{
    IoResult const res = file->read (mem, ret_nread);
    switch (res) {
	case IoResult::Normal:
	    return AsyncIoResult::Normal;
	case IoResult::Eof:
	    return AsyncIoResult::Eof;
	case IoResult::Error:
	    return AsyncIoResult::Error;
    }

    unreachable ();
    return AsyncIoResult::Error;
}

mt_throws AsyncIoResult
FileConnection::write (
#ifdef LIBMARY_WIN32_IOCP
                       OVERLAPPED  * const mt_nonnull /* overlapped */,
#endif
                       ConstMemory   const mem,
		       Size        * const ret_nwritten)
{
    if (!file->write (mem, ret_nwritten))
	return AsyncIoResult::Error;

    if (!file->flush ())
	return AsyncIoResult::Error;

    return AsyncIoResult::Normal;
}

mt_throws AsyncIoResult
FileConnection::writev (
#ifdef LIBMARY_WIN32_IOCP
                        OVERLAPPED   * const mt_nonnull /* overlapped */,
                        WSABUF       * const mt_nonnull buffers,
#else
                        struct iovec * const iovs,
#endif
			Count          const num_iovs,
			Size         * const ret_nwritten)
{
#ifdef LIBMARY_WIN32_IOCP
    struct iovec iovs [num_iovs];
    for (Count i = 0; i < num_iovs; ++i) {
        iovs [i].iov_base = buffers [i].buf;
        iovs [i].iov_len  = buffers [i].len;
    }
#endif

    Result const res = file->writev (iovs, num_iovs, ret_nwritten);
    if (!res)
	return AsyncIoResult::Error;

    return AsyncIoResult::Normal;
}

}

