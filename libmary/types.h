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


#ifndef __LIBMARY__TYPES__H__
#define __LIBMARY__TYPES__H__


#include <libmary/types_base.h>


#ifdef __GNUC__
#define mt_likely(x)   __builtin_expect(!!(x), 1)
#define mt_unlikely(x) __builtin_expect(!!(x), 0)
#endif


namespace M {

typedef void (*VoidFunction) (void);
typedef void (GenericCallback) (void *cb_data);

class EmptyBase {};

// TODO Move to log.h, append current session id string (log_prefix)
// Evil macro to save a few keystrokes for logE_ (_func, ...)

// _func2 and _func3 are a workaround to stringify __LINE__.

#define _func3(line)								 		\
	__FILE__,										\
	":" #line,								 		\
	":", __func__, ":",									\
	ConstMemory ("                                         " /* 41 spaces */, 		\
		     sizeof (__FILE__) + sizeof (#line) + sizeof (__func__) + 3 < 40 + 1 ?	\
			     40 - sizeof (__FILE__) - sizeof (#line) - sizeof (__func__) - 3 + 1 : 1)

#define _func3_(line)								 		\
	__FILE__,										\
	":" #line,								 		\
	":", __func__,										\
	ConstMemory ("                                         " /* 40 spaces */, 		\
		     sizeof (__FILE__) + sizeof (#line) + sizeof (__func__) + 2 < 39 + 1 ?	\
			     39 - sizeof (__FILE__) - sizeof (#line) - sizeof (__func__) - 2 + 1 : 1)

#if 0
// _func2 and _func3 are a workaround to stringify __LINE__.
#define _func3(line)								\
	__FILE__,								\
	ConstMemory ("                    " /* 20 spaces */,			\
		     sizeof (__FILE__) < 20 ? 20 - sizeof (__FILE__) : 0),	\
	":" #line,								\
	ConstMemory ("    " /* 5 spaces */,					\
		     sizeof (#line) < 5 ? 5 - sizeof (#line) : 0),		\
	":", __func__
#endif

// No line padding  #define _func3(line) __FILE__ ":" #line ":", __func__
#define _func2(line)  _func3(line)
#define _func2_(line) _func3_(line)

#define _func  _func2(__LINE__)
#define _func_ _func2_(__LINE__)

class Result
{
public:
    enum Value {
	Failure = 0,
	Success = 1
    };
    operator Value () const { return value; }
    Result (Value const value) : value (value) {}
    Result () {}
private:
    Value value;
};

// One should be able to write if (!compare()), whch should mean the same as
// if (compare() == ComparisonResult::Equal).
//
class ComparisonResult
{
public:
    enum Value {
	Less    = -1,
	Equal   = 0,
	Greater = 1
    };
    operator Value () const { return value; }
    ComparisonResult (Value const value) : value (value) {}
    ComparisonResult () {}
private:
    Value value;
};

// TODO For writes, Eof is an error condition, actually.
//      It will _always_ be an error condition.
//      Eof for writes may be detected by examining 'exc'.
class IoResult
{
public:
    enum Value {
	Normal = 0,
	Eof,
	Error
    };
    operator Value () const { return value; }
    IoResult (Value const value) : value (value) {}
    IoResult () {}
private:
    Value value;
};

class SeekOrigin
{
public:
    enum Value {
	Beg = 0,
	Cur,
	End
    };
    operator Value () const { return value; }
    SeekOrigin (Value const value) : value (value) {}
    SeekOrigin () {}
private:
    Value value;
};

#ifdef LIBMARY_PLATFORM_WIN32
struct iovec {
    void   *iov_base;
    size_t  iov_len;
};

#define IOV_MAX 1024
#endif

}


#include <libmary/memory.h>


namespace M {

class Format;

class AsyncIoResult
{
public:
    enum Value {
	Normal,
	// We've got the data and we know for sure that the following call to
	// read() will return EAGAIN.
	Normal_Again,
	// Normal_Eof is usually returned when we've received Hup event for
	// the connection, but there was some data to read.
	Normal_Eof,
	Again,
	Eof,
	Error
    };
    operator Value () const { return value; }
    AsyncIoResult (Value const value) : value (value) {}
    AsyncIoResult () {}
    Size toString_ (Memory const &mem, Format const &fmt);
private:
    Value value;
};

}


// For class Format.
#include <libmary/util_str_base.h>


#endif /* __LIBMARY__TYPES__H__ */

