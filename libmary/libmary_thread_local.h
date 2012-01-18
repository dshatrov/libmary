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


#ifndef __LIBMARY__LIBMARY_THREAD_LOCAL__H__
#define __LIBMARY__LIBMARY_THREAD_LOCAL__H__


#include <libmary/types.h>
#include <time.h>
#include <sys/uio.h>

#include <libmary/exception_buffer.h>


#ifdef LIBMARY_TLOCAL
#define LIBMARY_TLOCAL_SPEC LIBMARY_TLOCAL
#else
#define LIBMARY_TLOCAL_SPEC
#endif


namespace M {

class Exception;

class CodeReferenced;
class Object;

// DeferredConnectionSender's mwritev data.
class LibMary_MwritevData
{
public:
    bool           initialized;
    int           *fds;
    struct iovec **iovs;
    struct iovec  *iovs_heap;
    int           *num_iovs;
    int           *res;

    LibMary_MwritevData ()
	: initialized (false)
    {
    }
};

class LibMary_ThreadLocal
{
public:
    Object *deletion_queue;
    bool deletion_queue_processing;

    Count state_mutex_counter;

    ExceptionBuffer exc_buffer;
    Exception *exc;
    Uint32 exc_block;

    Object *last_coderef_container;

#ifndef PLATFORM_WIN32
    char *strerr_buf;
    Size strerr_buf_size;
#endif

  // Time-related data fields

    Time time_seconds;
    Time time_microseconds;
    Time unixtime;

    Time time_log_frac;

    struct tm localtime;
    Time saved_unixtime;
    // Saved monotonic clock value in seconds.
    Time saved_monotime;

    char timezone_str [5];

    LibMary_MwritevData mwritev;

    LibMary_ThreadLocal ();
    ~LibMary_ThreadLocal ();
};

#ifndef LIBMARY_MT_SAFE
extern LibMary_ThreadLocal _libMary_tlocal;
#endif

#ifdef LIBMARY_MT_SAFE
// Never returns NULL.
LibMary_ThreadLocal* libMary_getThreadLocal ();
#else
static inline LibMary_ThreadLocal* libMary_getThreadLocal ()
{
    return &_libMary_tlocal;
}
#endif

void libMary_threadLocalInit ();

}


#include <libmary/object.h>


#endif /* __LIBMARY__LIBMARY_THREAD_LOCAL__H__ */

