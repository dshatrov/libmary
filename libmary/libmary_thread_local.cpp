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


//#include <glib/gthread.h>
#include <glib.h>

#include <libmary/libmary_thread_local.h>


namespace M {

#ifdef LIBMARY_MT_SAFE
static GPrivate *tlocal_gprivate = NULL;
#else
LibMary_ThreadLocal _libMary_tlocal;
#endif

LibMary_ThreadLocal::LibMary_ThreadLocal ()
    : deletion_queue (NULL),
      deletion_queue_processing (false),
      state_mutex_counter (0),
      exc_buffer (1024 /* alloc_len */),
      exc (NULL),
      exc_block (0),
      last_coderef_container (NULL),

      time_seconds (0),
      time_microseconds (0),
      unixtime (0),

      saved_unixtime (0),
      saved_monotime (0)
{
    memset (&localtime, 0, sizeof (localtime));
    memset (timezone_str, ' ', sizeof (timezone_str));

#ifndef PLATFORM_WIN32
    strerr_buf_size = 4096;
    strerr_buf = new char [strerr_buf_size];
    assert (strerr_buf);
#endif
}

LibMary_ThreadLocal::~LibMary_ThreadLocal ()
{
#ifndef PLATFORM_WIN32
    delete[] strerr_buf;
#endif
}

#ifdef LIBMARY_MT_SAFE
// TODO Make use of LIBMARY_TLOCAL (native tlocal keyword)
LibMary_ThreadLocal*
libMary_getThreadLocal ()
{
    LibMary_ThreadLocal *tlocal =
	    static_cast <LibMary_ThreadLocal*> (g_private_get (tlocal_gprivate));
    if (!tlocal) {
	tlocal = new LibMary_ThreadLocal;
	g_private_set (tlocal_gprivate, tlocal);
    }

    return tlocal;
}
#endif

#ifdef LIBMARY_MT_SAFE
static void
tlocal_destructor (gpointer const _tlocal)
{
    LibMary_ThreadLocal * const tlocal = static_cast <LibMary_ThreadLocal*> (_tlocal);
    if (tlocal)
	delete tlocal;
}
#endif

void
libMary_threadLocalInit ()
{
#ifdef LIBMARY_MT_SAFE
    tlocal_gprivate = g_private_new (tlocal_destructor);
#endif
}

}

