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


#ifndef __LIBMARY__LOG__H__
#define __LIBMARY__LOG__H__


#include <libmary/types.h>
#include <libmary/exception.h>
#include <libmary/mutex.h>
#include <libmary/output_stream.h>
#include <libmary/util_time.h>


namespace M {

extern OutputStream *logs;

class LogLevel
{
public:
    enum Value {
	All     =  1000,
	Debug   =  2000,
	Info    =  3000,
	Warning =  4000,
	Error   =  5000,
	High    =  6000,
	Failure =  7000,
	None    = 10000,
	// Short loglevel name aliases are useful to enable/disable certain
	// loglevels from source quickly. Don't use them if you don't need
	// to flip between loglevels from time to time.
	A = All,
	D = Debug,
	I = Info,
	W = Warning,
	E = Error,
	H = High,
	F = Failure,
	N = None
    };
    operator Value () const { return value; }
    LogLevel (Value const value) : value (value) {}
    LogLevel () {}
private:
    Value value;
};

class LogGroup
{
private:
    unsigned loglevel;

public:
    void setLogLevel (unsigned const loglevel) { this->loglevel = loglevel; }
    unsigned getLogLevel () { return loglevel; }

    LogGroup (ConstMemory const &group_name, unsigned loglevel);
};

extern LogGroup libMary_logGroup_default;

static inline unsigned getDefaultLogLevel ()
{
    return libMary_logGroup_default.getLogLevel ();
}

static inline bool defaultLogLevelOn (unsigned const loglevel)
{
    return loglevel >= getDefaultLogLevel();
}

#define logLevelOn(group, loglevel)	\
	((loglevel) >= libMary_logGroup_ ## group .getLogLevel())

extern Mutex _libMary_log_mutex;

static inline void logLock ()
{
    _libMary_log_mutex.lock ();
}

static inline void logUnlock ()
{
    _libMary_log_mutex.unlock ();
}

// Note that it is possible to substitute variadic templates with a number of
// plaina templates while preserving the same calling syntax.

// TODO Roll this into new va-arg logs->print().
static inline void _libMary_do_log_unlocked (Format const & /* fmt */)
{
  // No-op
}

template <class T, class ...Args>
void _libMary_do_log_unlocked (Format const &fmt, T const &value, Args const &...args)
{
    logs->print_ (value, fmt);
    _libMary_do_log_unlocked (fmt, args...);
}

template <class ...Args>
void _libMary_do_log_unlocked (Format const & /* fmt */, Format const &new_fmt, Args const &...args)
{
    _libMary_do_log_unlocked (new_fmt, args...);
}

template <char const (&loglevel_str) [5], class ...Args>
void _libMary_log_unlocked (Args const &...args)
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    Format fmt;
    fmt.min_digits = 2;
    _libMary_do_log_unlocked (fmt, tlocal->localtime.tm_hour, ":", tlocal->localtime.tm_min, ":", tlocal->localtime.tm_sec, loglevel_str, fmt_def, args...);
    logs->flush ();
}

template <char const (&loglevel_str) [5], class ...Args>
void _libMary_log (Args const &...args)
{
    logLock ();
    _libMary_log_unlocked<loglevel_str> (args...);
    logUnlock ();
}

// Note that it is possible to substitute variadic macros with other language
// constructs while preserving the same calling syntax.

extern char const _libMary_loglevel_str_A [5];
extern char const _libMary_loglevel_str_D [5];
extern char const _libMary_loglevel_str_I [5];
extern char const _libMary_loglevel_str_W [5];
extern char const _libMary_loglevel_str_E [5];
extern char const _libMary_loglevel_str_H [5];
extern char const _libMary_loglevel_str_F [5];
extern char const _libMary_loglevel_str_N [5];

// Macros allows to avoid evaluation of the args if we're not going to put
// the message into the log.

// TODO Inlining this huge switch() statement for every invocation of log() is insane.
#define _libMary_log_macro(log_func, group, loglevel, ...)			\
	do {									\
	    if (mt_unlikely ((loglevel) >= libMary_logGroup_ ## group .getLogLevel())) {	\
		exc_block ();							\
		switch (loglevel) {						\
		    case LogLevel::All:						\
			log_func<_libMary_loglevel_str_A> (__VA_ARGS__);	\
			break;							\
		    case LogLevel::Debug:					\
			log_func<_libMary_loglevel_str_D> (__VA_ARGS__);	\
			break;							\
		    case LogLevel::Info:					\
			log_func<_libMary_loglevel_str_I> (__VA_ARGS__);	\
			break;							\
		    case LogLevel::Warning:					\
			log_func<_libMary_loglevel_str_W> (__VA_ARGS__);	\
			break;							\
		    case LogLevel::Error:					\
			log_func<_libMary_loglevel_str_E> (__VA_ARGS__);	\
			break;							\
		    case LogLevel::High:					\
			log_func<_libMary_loglevel_str_H> (__VA_ARGS__);	\
			break;							\
		    case LogLevel::Failure:					\
			log_func<_libMary_loglevel_str_F> (__VA_ARGS__);	\
			break;							\
		    case LogLevel::None:					\
			log_func<_libMary_loglevel_str_N> (__VA_ARGS__);	\
			break;							\
		    default:							\
			unreachable ();						\
		}								\
		exc_unblock ();							\
	    }									\
	} while (0)
#if 0
#define _libMary_log_macro(log_func, group, loglevel, ...)			\
	do {									\
	    if ((loglevel) >= libMary_logGroup_ ## group .getLogLevel()) {	\
		exc_block ();							\
		char const (*loglevel_str) [4];					\
		switch (loglevel) {						\
		    case LogLevel::All:						\
			loglevel_str = &"] A ";					\
			break;							\
		    case LogLevel::Debug:					\
			loglevel_str = &"] D ";					\
			break;							\
		    case LogLevel::Info:					\
			loglevel_str = &"] I ";					\
			break;							\
		    case LogLevel::Warning:					\
			loglevel_str = &"] W ";					\
			break;							\
		    case LogLevel::Error:					\
			loglevel_str = &"] E ";					\
			break;							\
		    case LogLevel::High:					\
			loglevel_str = &"] H ";					\
			break;							\
		    case LogLevel::Failure:					\
			loglevel_str = &"] F ";					\
			break;							\
		    case LogLevel::None:					\
			loglevel_str = &"] N ";					\
			break;							\
		    default:							\
			unreachable ();						\
		}								\
		log_func (*loglevel_str, __VA_ARGS__);				\
		exc_unblock ();							\
	    }									\
	} while (0)
#endif

#define _libMary_log_macro_s(log_func, group, loglevel, loglevel_str, ...)	\
	do {									\
	    if (mt_unlikely ((loglevel) >= libMary_logGroup_ ## group .getLogLevel())) {	\
		exc_block ();							\
		log_func<loglevel_str> (__VA_ARGS__);				\
		exc_unblock ();							\
	    }									\
	} while (0)

#define log(group, loglevel, ...)          _libMary_log_macro (_libMary_log,          group,   loglevel, __VA_ARGS__, "\n")
#define log_unlocked(group, loglevel, ...) _libMary_log_macro (_libMary_log_unlocked, group,   loglevel, __VA_ARGS__, "\n")
#define log_(loglevel, ...)                _libMary_log_macro (_libMary_log,          default, loglevel, __VA_ARGS__, "\n")
#define log_unlocked_(loglevel, ...)       _libMary_log_macro (_libMary_log_unlocked, default, loglevel, __VA_ARGS__, "\n")

#define logD(group, ...)          _libMary_log_macro_s (_libMary_log,          group,   LogLevel::Debug, _libMary_loglevel_str_D, __VA_ARGS__, "\n")
#define logD_unlocked(group, ...) _libMary_log_macro_s (_libMary_log_unlocked, group,   LogLevel::Debug, _libMary_loglevel_str_D, __VA_ARGS__, "\n")
#define logD_(...)                _libMary_log_macro_s (_libMary_log,          default, LogLevel::Debug, _libMary_loglevel_str_D, __VA_ARGS__, "\n")
#define logD_unlocked_(...)       _libMary_log_macro_s (_libMary_log_unlocked, default, LogLevel::Debug, _libMary_loglevel_str_D, __VA_ARGS__, "\n")

#define logI(group, ...)          _libMary_log_macro_s (_libMary_log,          group,   LogLevel::Info, _libMary_loglevel_str_I, __VA_ARGS__, "\n")
#define logI_unlocked(group, ...) _libMary_log_macro_s (_libMary_log_unlocked, group,   LogLevel::Info, _libMary_loglevel_str_I, __VA_ARGS__, "\n")
#define logI_(...)                _libMary_log_macro_s (_libMary_log,          default, LogLevel::Info, _libMary_loglevel_str_I, __VA_ARGS__, "\n")
#define logI_unlocked_(...)       _libMary_log_macro_s (_libMary_log_unlocked, default, LogLevel::Info, _libMary_loglevel_str_I, __VA_ARGS__, "\n")

#define logW(group, ...)          _libMary_log_macro_s (_libMary_log,          group,   LogLevel::Warning, _libMary_loglevel_str_W, __VA_ARGS__, "\n")
#define logW_unlocked(group, ...) _libMary_log_macro_s (_libMary_log_unlocked, group,   LogLevel::Warning, _libMary_loglevel_str_W, __VA_ARGS__, "\n")
#define logW_(...)                _libMary_log_macro_s (_libMary_log,          default, LogLevel::Warning, _libMary_loglevel_str_W, __VA_ARGS__, "\n")
#define logW_unlocked_(...)       _libMary_log_macro_s (_libMary_log_unlocked, default, LogLevel::Warning, _libMary_loglevel_str_W, __VA_ARGS__, "\n")

#define logE(group, ...)          _libMary_log_macro_s (_libMary_log,          group,   LogLevel::Error, _libMary_loglevel_str_E,  __VA_ARGS__, "\n")
#define logE_unlocked(group, ...) _libMary_log_macro_s (_libMary_log_unlocked, group,   LogLevel::Error, _libMary_loglevel_str_E, __VA_ARGS__, "\n")
#define logE_(...)                _libMary_log_macro_s (_libMary_log,          default, LogLevel::Error, _libMary_loglevel_str_E, __VA_ARGS__, "\n")
#define logE_unlocked_(...)       _libMary_log_macro_s (_libMary_log_unlocked, default, LogLevel::Error, _libMary_loglevel_str_E, __VA_ARGS__, "\n")

#define logH(group, ...)          _libMary_log_macro_s (_libMary_log,          group,   LogLevel::High, _libMary_loglevel_str_H, __VA_ARGS__, "\n")
#define logH_unlocked(group, ...) _libMary_log_macro_s (_libMary_log_unlocked, group,   LogLevel::High, _libMary_loglevel_str_H, __VA_ARGS__, "\n")
#define logH_(...)                _libMary_log_macro_s (_libMary_log,          default, LogLevel::High, _libMary_loglevel_str_H, __VA_ARGS__, "\n")
#define logH_unlocked_(...)       _libMary_log_macro_s (_libMary_log_unlocked, default, LogLevel::High, _libMary_loglevel_str_H, __VA_ARGS__, "\n")

#define logF(group, ...)          _libMary_log_macro_s (_libMary_log,          group,   LogLevel::Failure, _libMary_loglevel_str_F, __VA_ARGS__, "\n")
#define logF_unlocked(group, ...) _libMary_log_macro_s (_libMary_log_unlocked, group,   LogLevel::Failure, _libMary_loglevel_str_F, __VA_ARGS__, "\n")
#define logF_(...)                _libMary_log_macro_s (_libMary_log,          default, LogLevel::Failure, _libMary_loglevel_str_F, __VA_ARGS__, "\n")
#define logF_unlocked_(...)       _libMary_log_macro_s (_libMary_log_unlocked, default, LogLevel::Failure, _libMary_loglevel_str_F, __VA_ARGS__, "\n")

}


#endif /* __LIBMARY__LOG__H__ */

