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

#include <errno.h>
#include <time.h>

#include <libmary/log.h>
#include <libmary/util_str.h>
#ifndef LIBMARY_PLATFORM_WIN32
#include <libmary/posix.h>
#endif

#ifdef LIBMARY_PLATFORM_WIN32
#include <windows.h>
#endif

#include <libmary/util_time.h>

#include <glib.h>


#ifdef LIBMARY_PLATFORM_WIN32
#ifdef LIBMARY_WIN32_SECURE_CRT
extern "C" {
    typedef int errno_t;

    errno_t localtime_s (struct tm    *tm,
                         time_t const *time);

    errno_t gmtime_s (struct tm    *tm,
                      time_t const *time);
}
#endif
#endif


namespace M {

namespace {
LogGroup libMary_logGroup_time ("time", LogLevel::None);
}

mt_throws Result updateTime ()
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();

#ifdef LIBMARY_PLATFORM_WIN32
    DWORD const win_time_dw = timeGetTime();
    if (tlocal->prv_win_time_dw >= win_time_dw) {
        tlocal->win_time_offs += 0x100000000;
    }
    tlocal->prv_win_time_dw = win_time_dw;
    DWORD const win_time = win_time_dw + tlocal->win_time_offs;

    Time const new_seconds = (Time) win_time / 1000;
    Time const new_microseconds = (Time) win_time * 1000;

//    tlocal->time_log_frac = new_microseconds % 1000000 / 100;
#else
    struct timespec ts;
    // Note that clock_gettime is well-optimized on Linux x86_64 and does not carry
    // full syscall overhead (depends on system configuration).
    int const res = clock_gettime (CLOCK_MONOTONIC, &ts);
    if (res == -1) {
	exc_throw <PosixException> (errno);
	exc_push <InternalException> (InternalException::BackendError);
	logE_ (_func, "clock_gettime() failed: ", errnoString (errno));
	return Result::Failure;
    } else
    if (res != 0) {
	exc_throw <InternalException> (InternalException::BackendError);
	logE_ (_func, "clock_gettime(): unexpected return value: ", res);
	return Result::Failure;
    }

    logD (time, _func, "tv_sec: ", ts.tv_sec, ", tv_nsec: ", ts.tv_nsec);

    Time const new_seconds = ts.tv_sec;
    Time const new_microseconds = (Uint64) ts.tv_sec * 1000000 + (Uint64) ts.tv_nsec / 1000;

//    tlocal->time_log_frac = ts.tv_nsec % 1000000000 / 100000;
#endif

    tlocal->time_log_frac = new_microseconds % 1000000 / 100;

    if (new_seconds >= tlocal->time_seconds)
	tlocal->time_seconds = new_seconds;
    else
	logW_ (_func, "seconds backwards: ", new_seconds, " (was ", tlocal->time_seconds, ")");

    if (new_microseconds >= tlocal->time_microseconds)
	tlocal->time_microseconds = new_microseconds;
    else
	logW_ (_func, "microseconds backwards: ", new_microseconds, " (was ", tlocal->time_microseconds, ")");

//    logD_ (_func, "time_seconds: ", tlocal->time_seconds, ", time_microseconds: ", tlocal->time_microseconds);

    logD (time, _func, fmt_hex, tlocal->time_seconds, ", ", tlocal->time_microseconds);

    if (tlocal->saved_monotime < tlocal->time_seconds) {
	// Updading saved unixtime once in a minute.
	if (tlocal->time_seconds - tlocal->saved_monotime >= 60) {
	    // Obtaining current unixtime. This is an extra syscall.
	    tlocal->saved_unixtime = time (NULL);
	    tlocal->saved_monotime = tlocal->time_seconds;
	}

      // Updating localtime (broken-down time).

	time_t const cur_unixtime = tlocal->saved_unixtime + (tlocal->time_seconds - tlocal->saved_monotime);
	tlocal->unixtime = cur_unixtime;
	// Note that we call tzset() in libMary_posixInit() for localtime_r() to work correctly.
#ifdef LIBMARY_PLATFORM_WIN32
  #ifdef LIBMARY_WIN32_SECURE_CRT
        if (localtime_s (&tlocal->localtime, &cur_unixtime) != 0)
            logF_ (_func, "localtime_s() failed");
  #else
        libraryLock ();
        struct tm * const tmp_localtime = localtime (&cur_unixtime);
        if (tmp_localtime) {
            tlocal->localtime = *tmp_localtime;
        }
        libraryUnlock ();
        if (!tmp_localtime)
            logF_ (_func, "localtime() failed");
  #endif
#else
	localtime_r (&cur_unixtime, &tlocal->localtime);
#endif
    }

//    long const timezone_abs = timezone >= 0 ? timezone : -timezone;
    long const timezone_abs = 0;
//    tlocal->timezone_str [0] = timezone >= 0 ? '+' : '-';
    tlocal->timezone_str [0] = '+';
    tlocal->timezone_str [1] = '0' + timezone_abs / 36000;
    tlocal->timezone_str [2] = '0' + timezone_abs /  3600 % 10;
    tlocal->timezone_str [3] = '0' + timezone_abs /   600 %  6;
    tlocal->timezone_str [4] = '0' + timezone_abs /    60 % 10;

    return Result::Success;
}

void splitTime (Time        const unixtime,
		struct tm * const mt_nonnull ret_tm)
{
    time_t tmp_unixtime = (time_t) unixtime;
#ifdef LIBMARY_PLATFORM_WIN32
  #ifdef LIBMARY_WIN32_SECURE_CRT
    if (localtime_s (ret_tm, &tmp_unixtime) != 0)
      logF_ (_func, "localtime_s() failed");
  #else
    libraryLock ();
    struct tm * const tmp_localtime = localtime (&tmp_unixtime);
    if (tmp_localtime) {
        *ret_tm = *tmp_localtime;
    }
    libraryUnlock ();
    if (!tmp_localtime)
        logF_ (_func, "localtime() failed");
  #endif
#else
    localtime_r (&tmp_unixtime, ret_tm);
#endif
}

void uSleep (unsigned long const microseconds)
{
    g_usleep ((gulong) microseconds);
}

Size timeToString (Memory const &mem,
		   Time const time)
{
    static char const *days [] = {
	    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

    static char const *months [] = {
	    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
	    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

    time_t t = time;

    struct tm tm;
#ifdef LIBMARY_PLATFORM_WIN32
  #ifdef LIBMARY_WIN32_SECURE_CRT
    if (gmtime_s (&tm, &t) != 0)
  #else
    libraryLock ();
    struct tm * const tmp_tm = gmtime (&t);
    if (tmp_tm) {
        tm = *tmp_tm;
    }
    libraryUnlock ();
    if (!tmp_tm)
  #endif
#else
    if (!gmtime_r (&t, &tm))
#endif
    {
	logE_ (_func, "gmtime_r() failed");

	if (mem.len() > 0)
	    mem.mem() [0] = 0;

	return 0;
    }

    size_t const res = strftime ((char*) mem.mem(), mem.len(), "---, %d --- %Y %H:%M:%S GMT", &tm);
    if (res == 0 || res >= mem.len()) {
	logE_ (_func, "strftime() failed");

	if (mem.len() > 0)
	    mem.mem() [0] = 0;

	return 0;
    }

    assert (tm.tm_wday < (int) (sizeof (days) / sizeof (*days)));
    memcpy (mem.mem(), days [tm.tm_wday], 3);

    assert (tm.tm_mon < (int) (sizeof (months) / sizeof (*months)));
    memcpy (mem.mem() + 8, months [tm.tm_mon], 3);

    return (Size) res;
}

Result parseDuration (ConstMemory   const mem,
		      Time        * const mt_nonnull ret_duration)
{
    Size offs = 0;

    Uint32 num [3] = { 0, 0, 0 };

    int i;
    for (i = 0; i < 3; ++i) {
	while (offs < mem.len() && mem.mem() [offs] == ' ')
	    ++offs;

	Byte const *endptr;
	if (!strToUint32 (mem.region (offs), &num [i], &endptr, 10 /* base */))
	    return Result::Failure;

	offs = endptr - mem.mem();

	while (offs < mem.len() && mem.mem() [offs] == ' ')
	    ++offs;

	if (i < 2) {
	    if (mem.mem() [offs] != ':')
		break;

	    ++offs;
	}
    }

    while (offs < mem.len() && mem.mem() [offs] == ' ')
	++offs;

    if (offs != mem.len())
	return Result::Failure;

    switch (i) {
	case 0:
	    *ret_duration = (Time) num [0];
	    break;
	case 1:
	    *ret_duration = ((Time) num [0] * 60) + ((Time) num [1]);
	    break;
	case 2:
	    unreachable ();
	    break;
	case 3:
	    *ret_duration = ((Time) num [0] * 3600) + ((Time) num [1] * 60) + ((Time) num [2]);
	    break;
	default:
	    unreachable ();
    }

    return Result::Success;
}

}

