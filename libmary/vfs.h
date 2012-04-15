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


#ifndef __LIBMARY__VFS__H__
#define __LIBMARY__VFS__H__


#include <libmary/types.h>
#include <libmary/exception.h>
#include <libmary/basic_referenced.h>
#include <libmary/string.h>
#include <libmary/native_file.h>


namespace M {

class Vfs : public BasicReferenced
{
public:
    typedef NativeFile::FileType FileType;
    typedef NativeFile::FileStat FileStat;

    class Directory : public BasicReferenced
    {
    public:
	virtual mt_throws Result getNextEntry (Ref<String> &ret_name) = 0;

	virtual mt_throws Result rewind () = 0;
    };

    virtual mt_throws Ref<FileStat> stat (ConstMemory const &name) = 0;

    virtual mt_throws Ref<Directory> openDirectory (ConstMemory const &dirname) = 0;

#if 0
    virtual mt_throws Ref<File> openFile (ConstMemory const &filename,
					  Uint32             open_flags,
					  File::AccessMode   access_mode) = 0;
#endif

    static mt_throws Ref<Vfs> createDefaultLocalVfs (ConstMemory const &root_path);

    virtual ~Vfs () {}
};

}


#endif /* __LIBMARY__VFS__H__ */

