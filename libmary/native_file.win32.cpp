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


#include <libmary/native_file.h>


namespace M {

mt_throws IoResult
NativeFile::read (Memory   const mem,
		  Size   * const ret_nread)
{
    return IoResult::Normal;
}

mt_throws Result
NativeFile::write (ConstMemory   const mem,
		   Size        * const ret_nwritten)
{
    return Result::Success;
}

mt_throws Result
NativeFile::flush ()
{
    return Result::Success;
}

mt_throws Result
NativeFile::seek (FileOffset const offset,
		  SeekOrigin const origin)
{
    return Result::Success;
}

mt_throws Result
NativeFile::tell (FileSize * const ret_pos)
{
    return Result::Success;
}

mt_throws Result
NativeFile::sync ()
{
    return Result::Success;
}

mt_throws Result
NativeFile::close (bool const flush_data)
{
    return Result::Success;
}

void
NativeFile::resetFd ()
{
}

mt_throws Result
NativeFile::open (ConstMemory const filename,
		  Uint32      const open_flags,
		  AccessMode  const access_mode)
{
    return Result::Success;
}

NativeFile::NativeFile ()
{
}

NativeFile::~NativeFile ()
{
}

}

