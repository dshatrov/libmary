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


#ifndef __LIBMARY__VFS_POSIX__H__
#define __LIBMARY__VFS_POSIX__H__


#include <sys/types.h>
#include <dirent.h>

#include <libmary/vfs.h>


namespace M {

class VfsPosix : public Vfs
{
private:
    Ref<String> root_path;

    ConstMemory makePath (Ref<String> &str_holder,
			  ConstMemory const path_suffix);

    ConstMemory makePathCstr (Ref<String> &str_holder,
			      ConstMemory const path_suffix);

public:
    class Directory : public Vfs::Directory
    {
	friend class VfsPosix;

    private:
	DIR *dir;

	mt_throws Result open (ConstMemory const &dirname);

	Directory ();

    public:
	mt_throws Result getNextEntry (Ref<String> &ret_name);

	mt_throws Result rewind ();

	~Directory ();
    };

    mt_throws Ref<StatData> stat (ConstMemory const &name);

    mt_throws Ref<Vfs::Directory> openDirectory (ConstMemory const &dirname);

#if 0
    mt_throws Ref<File> openFile (ConstMemory const &filename,
				  Uint32             open_flags,
				  File::AccessMode   access_mode);
#endif

    VfsPosix (ConstMemory const &root_path);
};

}


#endif /* __LIBMARY__VFS_POSIX__H__ */

