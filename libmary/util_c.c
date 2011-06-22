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


#ifndef PLATFORM_WIN32
/* This is a workaround for an ugly API bug in the GNU C library:
 * portable strerror_r() function requires _GNU_SOURCE macro not
 * to be defined, but g++ defines it unconditionally.
 * The consequences of undef'ing it before including
 * <string.h> are uncertain and may well be fatal.
 */
#ifdef _GNU_SOURCE
#undef _GNU_SOURCE
#endif


/* See man strerror_r */
#define _XOPEN_SOURCE 600
#include <string.h>

#include <libmary/annotations.h>


int _libmary_strerror_r (int      const errnum,
			 char   * const mt_nonnull buf,
			 size_t   const buflen)
{
    return strerror_r (errnum, buf, buflen);
}
#endif // PLATFORM_WIN32

