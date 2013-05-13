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


#ifndef LIBMARY__FILE_CONNECTION__H__
#define LIBMARY__FILE_CONNECTION__H__


#include <libmary/types.h>
#include <libmary/connection.h>
#include <libmary/file.h>


namespace M {

//#warning For IOCP, this is completely inadequate.
//#warning For epoll, this is inadequate as well, because
//#warning we hold sender mutex while doing blocking i/o.
//#warning So this must be redesigned for both cases.

//#warning Продумать синхронизацию Sender'ов. Можно ли ввести разрыв мьютекса на время системного вызова?

class FileConnection : public Connection
{
private:
    mt_const File *file;

public:
  mt_iface (Connection)
    mt_iface (AsyncInputStream)
      mt_throws AsyncIoResult read (
#ifdef LIBMARY_WIN32_IOCP
                                    OVERLAPPED  * mt_nonnull overlapped,
#endif
                                    Memory       mem,
				    Size        *ret_nread);
    mt_iface_end

    mt_iface (AsyncOutputStream)
      mt_throws AsyncIoResult write (
#ifdef LIBMARY_WIN32_IOCP
                                     OVERLAPPED  * mt_nonnull overlapped,
#endif
                                     ConstMemory  mem,
				     Size        *ret_nwritten);

      mt_throws AsyncIoResult writev (
#ifdef LIBMARY_WIN32_IOCP
                                      OVERLAPPED   * mt_nonnull overlapped,
                                      WSABUF       * mt_nonnull buffers,
#else
                                      struct iovec *iovs,
#endif
				      Count         num_iovs,
				      Size         *ret_nwrittev);
    mt_iface_end

#ifdef LIBMARY_ENABLE_MWRITEV
    int getFd () { return file->getFd(); }
#endif
  mt_iface_end

    mt_const void init (File * const mt_nonnull file) { this->file = file; }

    FileConnection (Object * const coderef_container)
        : DependentCodeReferenced (coderef_container),
          file (NULL)
    {}
};

}


#endif /* LIBMARY__FILE_CONNECTION__H__ */

