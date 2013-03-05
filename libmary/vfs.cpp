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


#include <libmary/util_base.h>
#include <libmary/vfs_posix.h>
#include <libmary/log.h>

#include <libmary/vfs.h>


namespace M {

mt_throws Result
Vfs::createSubdirs (ConstMemory dirs_path)
{
//    logD_ (_func, "dirs_path: ", dirs_path);

    ConstMemory tail = dirs_path;
    for (;;) {
        ConstMemory cur_dir = dirs_path;
        Byte const * const slash = (Byte const *) memchr (tail.mem(), '/', tail.len());
        if (slash) {
            tail = tail.region (slash - tail.mem() + 1);
//            logD_ (_func, "tail: ", tail, ", cur_dir: ", cur_dir);
            cur_dir = dirs_path.region (0, slash - dirs_path.mem());
        }

        if (cur_dir.len() > 0) {
            if (!createDirectory (cur_dir))
                return Result::Failure;
        }

        if (!slash)
            break;
    }

    return Result::Success;
}

mt_throws Ref<Vfs>
Vfs::createDefaultLocalVfs (ConstMemory const root_path)
{
#ifdef LIBMARY_PLATFORM_WIN32
    abort ();
    return NULL;
#else
    // VfsPosix appears to work under mingw as well.
    return grab (static_cast <Vfs*> (new (std::nothrow) VfsPosix (root_path)));
#endif
}

}

