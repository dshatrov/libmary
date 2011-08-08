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


#include <errno.h>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <cmath>

#include <libmary/libmary_thread_local.h>

#include <libmary/util_str.h>


namespace M {

Format fmt_def;
Format fmt_hex (16 /* num_base */, 0 /* min_digits */, (unsigned) -1 /* precision */);

extern "C" {

/* See c_util.c for the explanation. */
int _libmary_strerror_r (int     errnum,
			 char   *buf,
			 size_t  buflen);

}

#ifndef PLATFORM_WIN32
Ref<String> errnoToString (int const errnum)
{
    char buf [4096];

    int const res = _libmary_strerror_r (errnum, buf, sizeof buf);
    if (res == -1) {
	if (errno == EINVAL)
	    return grab (new String ("Invalid error code"));

	if (errno == ERANGE)
	    return grab (new String ("[error message is too long]"));

	return grab (new String ("[strerror_r() failed]"));
    } else
    if (res != 0) {
	return grab (new String ("[unexpected return value from strerror_r()]"));
    }

    return grab (new String ((char const *) buf));
}

char const * errnoString (int const errnum)
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal ();

    int const res = _libmary_strerror_r (errnum, tlocal->strerr_buf, tlocal->strerr_buf_size);
    if (res == -1) {
	if (errno == EINVAL)
	    return "Invalid error code";

	if (errno == ERANGE)
	    return "[error message is too long]";

	return "[strerror_r() failed]";
    } else
    if (res != 0) {
	return "[unexpected return value from strerror_r()]";
    }

    return tlocal->strerr_buf;
}
#endif // PLATFORM_WIN32

Ref<String> catenateStrings (ConstMemory const &left,
			     ConstMemory const &right)
{
    Ref<String> str = grab (new String (left.len() + right.len()));
    memcpy (str->mem().mem(), left.mem(), left.len());
    memcpy (str->mem().mem() + left.len(), right.mem(), right.len());
    return str;
}

ComparisonResult compare (ConstMemory const &left,
			  ConstMemory const &right)
{
    if (left.len() >= right.len()) {
	int const res = memcmp (left.mem(), right.mem(), right.len());
	if (res == 0) {
	    if (left.len() > right.len())
		return ComparisonResult::Greater;

	    return ComparisonResult::Equal;
	}

	if (res > 0)
	    return ComparisonResult::Greater;

	return ComparisonResult::Less;
    }

    int const res = memcmp (left.mem(), right.mem(), right.len());
    if (res == 0)
	return ComparisonResult::Less;

    if (res > 0)
	return ComparisonResult::Greater;

    return ComparisonResult::Less;
}

unsigned long strToUlong (ConstMemory const &mem)
{
    Byte tmp_str [64];
    if (mem.len() < sizeof (tmp_str)) {
	memcpy (tmp_str, mem.mem(), mem.len());
	tmp_str [mem.len()] = 0;
    } else {
	memcpy (tmp_str, mem.mem(), sizeof (tmp_str) - 1);
	tmp_str [sizeof (tmp_str) - 1] = 0;
    }

    return strtoul ((char const *) tmp_str, NULL /* endptr */, 10);
}

static ConstMemory strTo_stripWhitespace (ConstMemory const &mem)
{
    Byte const *begin = mem.mem();
    Size len = mem.len();

    {
	Size const i_end = len;
	for (Size i = 0; i < i_end; ++i) {
	    if (!isspace (begin [i]))
		break;

	    ++begin;
	    --len;
	}
    }

    {
	Size const orig_len = len;
	for (Size i = 1; i <= orig_len; ++i) {
	    if (!isspace (begin [orig_len - i]))
		break;

	    --len;
	}
    }

    return ConstMemory (begin, len);
}

mt_throws Result strToInt32_safe (char const * const cstr,
				  Int32 * const ret_val,
				  int const base)
{
    if (!cstr || *cstr == 0) {
	exc_throw <NumericConversionException> (NumericConversionException::EmptyString);
	return Result::Failure;
    }

    char *endptr = (char*) cstr;
    long long_val = strtol (cstr, &endptr, base);

    if (*endptr != 0) {
	exc_throw <NumericConversionException> (NumericConversionException::NonNumericChars);
	return Result::Failure;
    }

    if (long_val == LONG_MIN ||
	long_val == LONG_MAX)
    {
	// Note that MT-safe errno implies useless overhead of accessing
	// thread-local storage.
	if (errno == EINVAL || errno == ERANGE) {
	    exc_throw <PosixException> (errno);
	    exc_push <NumericConversionException> (NumericConversionException::Overflow);
	    return Result::Failure;
	}
    }

    if (long_val < Int32_Min ||
	long_val > Int32_Max)
    {
	exc_throw <NumericConversionException> (NumericConversionException::Overflow);
	return Result::Failure;
    }

    *ret_val = (Int32) long_val;

    return Result::Success;
}

mt_throws Result strToInt32_safe (ConstMemory const &mem_,
				  Int32 * const ret_val,
				  int const base)
{
    ConstMemory const mem = strTo_stripWhitespace (mem_);

    Byte tmp_str [130];
    if (mem.len() >= sizeof (tmp_str)) {
	exc_throw <NumericConversionException> (NumericConversionException::NonNumericChars);
	return Result::Failure;
    }

    memcpy (tmp_str, mem.mem(), mem.len());
    tmp_str [mem.len()] = 0;

    return strToInt32_safe ((char const*) tmp_str, ret_val, base);
}

mt_throws Result strToInt64_safe (char const * const cstr,
				  Int64 * const ret_val,
				  int const base)
{
    if (!cstr || *cstr == 0) {
	exc_throw <NumericConversionException> (NumericConversionException::EmptyString);
	return Result::Failure;
    }

    char *endptr = (char*) cstr;
    long llong_val = strtoll (cstr, &endptr, base);

    if (*endptr != 0) {
	exc_throw <NumericConversionException> (NumericConversionException::NonNumericChars);
	return Result::Failure;
    }

    if (llong_val == LLONG_MIN ||
	llong_val == LLONG_MAX)
    {
	if (errno == EINVAL || errno == ERANGE) {
	    exc_throw <PosixException> (errno);
	    exc_push <NumericConversionException> (NumericConversionException::Overflow);
	    return Result::Failure;
	}
    }

    if (llong_val < Int64_Min ||
	llong_val > Int64_Max)
    {
	exc_throw <NumericConversionException> (NumericConversionException::Overflow);
	return Result::Failure;
    }

    *ret_val = (Int64) llong_val;

    return Result::Success;
}

mt_throws Result strToInt64_safe (ConstMemory const &mem_,
				  Int64 * const ret_val,
				  int const base)
{
    ConstMemory const mem = strTo_stripWhitespace (mem_);

    Byte tmp_str [130];
    if (mem.len() >= sizeof (tmp_str)) {
	exc_throw <NumericConversionException> (NumericConversionException::NonNumericChars);
	return Result::Failure;
    }

    memcpy (tmp_str, mem.mem(), mem.len());
    tmp_str [mem.len()] = 0;

    return strToInt64_safe ((char const*) tmp_str, ret_val, base);
}

mt_throws Result strToUint32_safe (char const *cstr,
				   Uint32 * const ret_val,
				   int const base)
{
    if (!cstr || *cstr == 0) {
	exc_throw <NumericConversionException> (NumericConversionException::EmptyString);
	return Result::Failure;
    }

    char *endptr = (char*) cstr;
    unsigned long ulong_val = strtoul (cstr, &endptr, base);

    if (*endptr != 0) {
	exc_throw <NumericConversionException> (NumericConversionException::NonNumericChars);
	return Result::Failure;
    }

    if (ulong_val == ULONG_MAX) {
	if (errno == EINVAL || errno == ERANGE) {
	    exc_throw <PosixException> (errno);
	    exc_push <NumericConversionException> (NumericConversionException::Overflow);
	    return Result::Failure;
	}
    }

    if (ulong_val > Uint32_Max) {
	exc_throw <NumericConversionException> (NumericConversionException::Overflow);
	return Result::Failure;
    }

    *ret_val = (Uint32) ulong_val;

    return Result::Success;
}

mt_throws Result strToUint32_safe (ConstMemory const &mem_,
				   Uint32 * const ret_val,
				   int const base)
{
    ConstMemory const mem = strTo_stripWhitespace (mem_);

    Byte tmp_str [130];
    if (mem.len() >= sizeof (tmp_str)) {
	exc_throw <NumericConversionException> (NumericConversionException::NonNumericChars);
	return Result::Failure;
    }

    memcpy (tmp_str, mem.mem(), mem.len());
    tmp_str [mem.len()] = 0;

    return strToUint32_safe ((char const*) tmp_str, ret_val, base);
}

mt_throws Result strToUint64_safe (char const *cstr,
				   Uint64 * const ret_val,
				   int const base)
{
    if (!cstr || *cstr == 0) {
	exc_throw <NumericConversionException> (NumericConversionException::EmptyString);
	return Result::Failure;
    }

    char *endptr = (char*) cstr;
    unsigned long long ullong_val = strtoull (cstr, &endptr, base);

    if (*endptr != 0) {
	exc_throw <NumericConversionException> (NumericConversionException::NonNumericChars);
	return Result::Failure;
    }

    if (ullong_val == ULLONG_MAX) {
	if (errno == EINVAL || errno == ERANGE) {
	    exc_throw <PosixException> (errno);
	    exc_push <NumericConversionException> (NumericConversionException::Overflow);
	    return Result::Failure;
	}
    }

    if (ullong_val > Uint64_Max) {
	exc_throw <NumericConversionException> (NumericConversionException::Overflow);
	return Result::Failure;
    }

    *ret_val = (Uint64) ullong_val;

    return Result::Success;
}

mt_throws Result strToUint64_safe (ConstMemory const &mem_,
				   Uint64 * const ret_val,
				   int const base)
{
    ConstMemory const mem = strTo_stripWhitespace (mem_);

    Byte tmp_str [130];
    if (mem.len() >= sizeof (tmp_str)) {
	exc_throw <NumericConversionException> (NumericConversionException::NonNumericChars);
	return Result::Failure;
    }

    memcpy (tmp_str, mem.mem(), mem.len());
    tmp_str [mem.len()] = 0;

    return strToUint64_safe ((char const*) tmp_str, ret_val, base);
}

mt_throws Result strToDouble_safe (char const * const cstr,
				   double * const ret_val)
{
    if (!cstr || *cstr == 0) {
	exc_throw <NumericConversionException> (NumericConversionException::EmptyString);
	return Result::Failure;
    }

    char *endptr = (char*) cstr;
    double double_val= strtod (cstr, &endptr);

    if (*endptr != 0) {
	exc_throw <NumericConversionException> (NumericConversionException::NonNumericChars);
	return Result::Failure;
    }

    if (double_val ==  0 /* Underflow */ ||
	double_val == -HUGE_VAL ||
	double_val ==  HUGE_VAL)
    {
	if (errno == ERANGE) {
	    exc_throw <PosixException> (errno);
	    exc_push <NumericConversionException> (NumericConversionException::Overflow);
	    return Result::Failure;
	}
    }

    *ret_val = double_val;

    return Result::Success;
}

mt_throws Result strToDouble_safe (ConstMemory const &mem_,
				   double * const ret_val)
{
    ConstMemory const mem = strTo_stripWhitespace (mem_);

    // What's max string length for a double?
    Byte tmp_str [1024];
    if (mem.len() >= sizeof (tmp_str)) {
	exc_throw <NumericConversionException> (NumericConversionException::NonNumericChars);
	return Result::Failure;
    }

    memcpy (tmp_str, mem.mem(), mem.len());
    tmp_str [mem.len()] = 0;

    return strToDouble_safe ((char const*) tmp_str, ret_val);
}

}

