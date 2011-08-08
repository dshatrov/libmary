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


#include "local_config.h"

#include <libmary/libmary.h>

#include <libmary/libmary_thread_local.h>

#ifdef PLATFORM_WIN32
#include <libmary/win32.h>
#else
#include <libmary/posix.h>
#endif


namespace M {

//File *outf;
//File *errf;

OutputStream *outs;
OutputStream *errs;
OutputStream *logs;

void libMaryInit ()
{
    {
	static bool initialized = false;

	if (initialized) {
	    return;
	}
	initialized = true;
    }

#if 0
// Deprecated
    // TODO myCppInit() should be immune to being called multiple times
    //      (with respect to g_thread_init() as well).
    MyCpp::myCppInit ();
#endif

    if (!g_thread_get_initialized ())
	g_thread_init (NULL);

#if 0
#if defined LIBMARY_MTSAFE && !defined LIBMARY_TLOCAL
#else
#error This is wrong!
    _libMary_exc_buf = new ExceptionBuffer (1024 /* alloc_len */);
#endif
#endif

    libMary_threadLocalInit ();

#ifdef PLATFORM_WIN32
    libMary_win32Init ();
#else
    libMary_posixInit ();
#endif
}

}

