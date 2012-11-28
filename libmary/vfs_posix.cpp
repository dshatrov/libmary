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


#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>

#include <libmary/array_holder.h>
#include <libmary/native_file.h>
#include <libmary/log.h>
#include <libmary/util_dev.h>
#include <libmary/util_posix.h>

#include <libmary/vfs_posix.h>


namespace M {

/* NOTE: glib functions from the "File Utilities" section are not
 * thread safe and cannot be used here (as of glib-2.12.13). */

ConstMemory
VfsPosix::makePath (Ref<String> &str_holder,
		    ConstMemory const path_suffix)
{
    Memory const root_mem = root_path->mem();
    if (root_mem.len()) {
	if (root_mem.mem() [root_mem.len() - 1] == '/')
	    str_holder = catenateStrings (root_mem, path_suffix);
	else
	    str_holder = makeString (root_mem, "/", path_suffix);

	return str_holder->mem();
    }

    return path_suffix;
}

ConstMemory
VfsPosix::makePathCstr (Ref<String> &str_holder,
			ConstMemory const path_suffix)
{
    Memory const root_mem = root_path->mem();
    if (root_mem.len()) {
	if (root_mem.mem() [root_mem.len() - 1] == '/')
	    str_holder = catenateStrings (root_mem, path_suffix);
	else
	    str_holder = makeString (root_mem, "/", path_suffix);
    } else {
	str_holder = grab (new String (path_suffix));
    }

    return str_holder->mem();
}

mt_throws Result
VfsPosix::Directory::getNextEntry (Ref<String> &ret_str)
{
    ret_str = NULL;

#ifdef LIBMARY_PLATFORM_WIN32
    errno = 0;
    libraryLock ();
    struct dirent * const dirent = readdir (dir);
    if (!dirent) {
        libraryUnlock ();
        if (errno == 0)
            return Result::Success;

        exc_throw <PosixException> (errno);
        return Result::Failure;
    }

    ret_str = grab (new String ((char const*) dirent->d_name));
    libraryUnlock ();
#else
    ArrayHolder<unsigned char> dirent_array (sizeof (struct dirent) + NAME_MAX + 1);
    struct dirent * const dirent = (struct dirent*) &dirent_array [0];
    struct dirent *retp;

    int res = readdir_r (dir, dirent, &retp);
    if (res != 0) {
	exc_throw <PosixException> (res);
	return Result::Failure;
    }

    if (retp == NULL)
	return Result::Success;

    if (retp != dirent) {
	exc_throw <InternalException> (InternalException::BackendMalfunction);
	return Result::Failure;
    }

    ret_str = grab (new String ((char const*) dirent->d_name));
#endif

    return Result::Success;
}

mt_throws Result
VfsPosix::Directory::rewind ()
{
    rewinddir (dir);
    return Result::Success;
}

mt_throws Result
VfsPosix::Directory::open (ConstMemory const &_dirname)
{
    char dirname [_dirname.len() + 1];
    memcpy (dirname, _dirname.mem(), _dirname.len());
    dirname [_dirname.len()] = 0;

//    logD_ (_func, "opening ", ConstMemory (dirname, sizeof (dirname)));

    dir = opendir (dirname);
    if (dir == NULL) {
	exc_throw <PosixException> (errno);
	return Result::Failure;
    }

    return Result::Success;
}

VfsPosix::Directory::Directory ()
    : dir (NULL)
{
}

VfsPosix::Directory::~Directory ()
{
    if (dir) {
	for (;;) {
	    int res = closedir (dir);
	    if (res == -1) {
		if (errno == EINTR)
		    continue;

		logE_ (_func, "closedir() failed: ", errnoString (errno));
	    } else
	    if (res != 0) {
		logE_ (_func, "closedir(): unexpected return value: ", res);
	    }

	    break;
	}
    }
}

mt_throws Result
VfsPosix::stat (ConstMemory   const _name,
                FileStat    * const mt_nonnull ret_stat)
{
    Ref<String> name_str;
    ConstMemory const name = makePathCstr (name_str, _name);

    struct stat stat_buf;

    int const res = ::stat ((char const*) name.mem(), &stat_buf);
    if (res == -1) {
	exc_throw <PosixException> (errno);
	return Result::Failure;
    } else
    if (res != 0) {
	exc_throw <InternalException> (InternalException::BackendMalfunction);
        return Result::Failure;
    }

    return posix_statToFileStat (&stat_buf, ret_stat);
}

mt_throws Ref<Vfs::Directory>
VfsPosix::openDirectory (ConstMemory const &_dirname)
{
    Ref<String> dirname_str;
    ConstMemory const dirname = makePath (dirname_str, _dirname);

    Ref<Directory> directory = grab (new Directory);
//    if (!directory->open (makeString (root_path->mem(), "/", dirname)->mem()))
    if (!directory->open (dirname))
	return NULL;

    return directory;
}

#if 0
mt_throws Ref<File>
VfsPosix::openFile (ConstMemory      const &_filename,
		    Uint32           const  open_flags,
		    File::AccessMode const  access_mode)
{
    Ref<String> filename_str;
    ConstMemory const filename = makePath (filename_str, _filename);
// TODO
#if 0
    try {
	return grab (static_cast <File*> (new NativeFile (filename->getData (), open_flags, access_mode)));
    } catch (Exception &exc) {
	throw InternalException (String::nullString (), exc.clone ());
    }
#endif
    unreachable ();
    return NULL;
}
#endif

VfsPosix::VfsPosix (ConstMemory const &root_path)
    : root_path (grab (new String (root_path)))
{
}

}

