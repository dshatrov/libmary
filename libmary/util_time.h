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


#ifndef __LIBMARY__UTIL_TIME__H__
#define __LIBMARY__UTIL_TIME__H__


#include <libmary/types.h>

#include <time.h>


namespace M {

// TODO 1 раз в минуту делать gettimeofday и поддерживать реальное время дня (для логов).

typedef uint64_t Time;

// TODO Must be thread-safe.
extern Time _libMary_time_seconds;
extern Time _libMary_time_microseconds;
extern Time _libMary_unixtime;

// TODO Must be thread-safe.
extern struct tm _libMary_localtime;

// Retreives cached time in seconds.
static inline Time getTime ()
{
    return _libMary_time_seconds;
}

static inline Time getTimeMilliseconds ()
{
    return _libMary_time_microseconds / 1000;
}

static inline Time getUnixtime ()
{
    return _libMary_unixtime;
}

// Retreives cached time in microseconds.
#if 0
// [Deprecated comment]
// Retreives the number of microseconds elapsed for the current second.
// Must be added to the numer of seconds returned by getTime() to get
// the actual real monotonic time in microseconds.
#endif
static inline Time getTimeMicroseconds ()
{
    return _libMary_time_microseconds;
}

mt_throws Result updateTime ();

void uSleep (unsigned long const microseconds);

static inline void sSleep (unsigned long const seconds)
{
    unsigned long const microseconds = seconds * 1000000;
    assert (microseconds > seconds);
    uSleep (microseconds);
}

enum {
    timeToString_BufSize = 30
};

Size timeToString (Memory const &buf,
		   Time time);

}


#endif /* __LIBMARY__UTIL_TIME__H__ */

