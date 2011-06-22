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


/* MyCpp - MyNC C++ helper library
 * Copyright (C) 2009-2011 Dmitry Shatrov
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __LIBMARY__VFS__H__
#define __LIBMARY__VFS__H__


#include <libmary/types.h>
#include <libmary/exception.h>
#include <libmary/object.h>
#include <libmary/string.h>
#include <libmary/file.h>


namespace M {

class Vfs : public Object
{
public:
    enum FileType
    {
	FileType_BlockDevice,
	FileType_CharacterDevice,
	FileType_Fifo,
	FileType_RegularFile,
	FileType_Directory,
	FileType_SymbolicLink,
	FileType_Socket
    };

    class StatData : public Referenced
    {
    public:
	unsigned long long size;
	FileType file_type;
    };

    class Directory : public Referenced
    {
    public:
	virtual mt_throws Result getNextEntry (Ref<String> &ret_name) = 0;

	virtual mt_throws Result rewind () = 0;
    };

    virtual mt_throws Ref<StatData> stat (ConstMemory const &name) = 0;

    virtual mt_throws Ref<Directory> openDirectory (ConstMemory const &dirname) = 0;

#if 0
    virtual mt_throws Ref<File> openFile (ConstMemory const &filename,
					  Uint32             open_flags,
					  File::AccessMode   access_mode) = 0;
#endif

    static mt_throws Ref<Vfs> createDefaultLocalVfs (ConstMemory const &root_path);
};

}


#endif /* __LIBMARY__VFS__H__ */

