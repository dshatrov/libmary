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


#include <libmary/types.h>
#include <cstdlib>
#include <cstdio>
#include <locale.h>


#include <libmary/libmary.h>

#include <libmary/libmary_thread_local.h>

#ifdef LIBMARY_PLATFORM_WIN32
#include <libmary/win32.h>
#else
#include <libmary/posix.h>
#endif

#ifdef LIBMARY_ENABLE_MWRITEV
#include <libmary/mwritev.h>
#endif


namespace M {

OutputStream *outs;
OutputStream *errs;
OutputStream *logs;

Stat *_libMary_stat;

void libMaryInit ()
{
    {
	static bool initialized = false;

	if (initialized) {
	    return;
	}
	initialized = true;
    }

    // Setting numeric locale for snprintf() to behave uniformly in all cases.
    // Specifically, we need dot ('.') to be used as a decimal separator.
    if (setlocale (LC_NUMERIC, "C") == NULL)
        fprintf (stderr, "WARNING: Could not set LC_NUMERIC locale to \"C\"\n");

    // GStreamer calls setlocale(LC_ALL, ""), which is lame. We fight this with setenv().
    if (setenv ("LC_NUMERIC", "C", 1 /* overwrite */) == -1)
        perror ("WARNING: Could not set LC_NUMERIC environment variable to \"C\"");

#ifdef LIBMARY_MT_SAFE
    if (!g_thread_get_initialized ())
	g_thread_init (NULL);
#endif

    _libMary_stat = new Stat;

#if !defined LIBMARY_MT_SAFE || defined LIBMARY_TLOCAL
    _libMary_exc_buf = new ExceptionBuffer (LIBMARY__EXCEPTION_BUFFER_SIZE);
#endif

    libMary_threadLocalInit ();

#ifdef LIBMARY_PLATFORM_WIN32
    libMary_win32Init ();
#else
    libMary_posixInit ();
#endif

#ifdef LIBMARY_ENABLE_MWRITEV
    libMary_mwritevInit ();
#endif

    randomSetSeed ((Uint32) getTime());
}

}

