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

class FileConnection : public Connection
{
private:
    mt_const File *file;

public:
    mt_iface (AsyncInputStream)
      mt_throws AsyncIoResult read (Memory  mem,
				    Size   *ret_nread);
    mt_iface_end

    mt_iface (AsyncOutputStream)
      mt_throws AsyncIoResult write (ConstMemory  mem,
				     Size        *ret_nwritten);

      mt_throws AsyncIoResult writev (struct iovec *iovs,
				      Count         num_iovs,
				      Size         *ret_nwrittev);
    mt_iface_end

    mt_iface (Connection)
#ifdef LIBMARY_ENABLE_MWRITEV
      int getFd () { return file->getFd(); }
#endif
    mt_iface_end

    mt_const void init (File * const mt_nonnull file) { this->file = file; }

    FileConnection () : file (NULL) {}
};

}


#endif /* LIBMARY__FILE_CONNECTION__H__ */

