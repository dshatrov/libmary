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


#ifndef __LIBMARY__NATIVE_FILE_LINUX__H__
#define __LIBMARY__NATIVE_FILE_LINUX__H__


#include <libmary/posix.h>

#include <libmary/file.h>


namespace M {

class NativeFile : public File
{
private:
    int fd;

public:
    mt_throws IoResult read (Memory const &mem,
			     Size *ret_nread);

    mt_throws Result write (ConstMemory const &mem,
			    Size *ret_nwritten);

    mt_throws Result seek (FileOffset offset,
			   SeekOrigin origin);

    mt_throws Result tell (FileSize *ret_pos);

    mt_throws Result flush ();

    mt_throws Result sync ();

    mt_throws Result close (bool flush_data = true);

    // Resets fd so that it won't be closed in the destructor.
    void resetFd ();

    // TODO Separate method mt_throws Result open().
    mt_throws NativeFile (ConstMemory const &filename,
			  Uint32             open_flags,
			  AccessMode         access_mode);

    NativeFile (int fd);

    ~NativeFile ();
};

}


#endif /* __LIBMARY__NATIVE_FILE_LINUX__H__ */

