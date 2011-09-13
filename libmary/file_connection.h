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


#ifndef __LIBMARY__FILE_CONNECTION__H__
#define __LIBMARY__FILE_CONNECTION__H__


#include <libmary/connection.h>
#include <libmary/file.h>


namespace M {

class FileConnection : public Connection
{
private:
    mt_const File *file;

public:
    mt_iface (AsyncInputStream)
    mt_begin

      mt_throws AsyncIoResult read (Memory const &mem,
				    Size         *ret_nread);

    mt_end

    mt_iface (AsyncOutputStream)
    mt_begin

      mt_throws AsyncIoResult write (ConstMemory const &mem,
				     Size              *ret_nwritten);

      mt_throws AsyncIoResult writev (struct iovec *iovs,
				      Count         num_iovs,
				      Size         *ret_nwrittev);

    mt_end

    mt_iface (Connection)
    mt_begin

      mt_throws Result close ();

#ifdef LIBMARY_ENABLE_MWRITEV
      int getFd ();
#endif

    mt_end

    mt_const void setFile (File *file);
};

}


#endif /* __LIBMARY__FILE_CONNECTION__H__ */

