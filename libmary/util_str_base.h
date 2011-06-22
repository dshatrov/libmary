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


#ifndef __LIBMARY__UTIL_STR_BASE__H__
#define __LIBMARY__UTIL_STR_BASE__H__


#include <libmary/types.h>

#include <cstdlib>
#include <cstring>
#include <cstdio>

#include <libmary/ref.h>
#include <libmary/string.h>


namespace M {

#ifndef PLATFORM_WIN32
Ref<String> errnoToString (int errnum);
char const* errnoString (int errnum);
#endif

Ref<String> catenateStrings (ConstMemory const &left,
			     ConstMemory const &right);


// ________________________________ toString() _________________________________

class Format
{
public:
    // Valid values: 10, 16.
    unsigned num_base;
    // 0 - not set.
    unsigned min_digits;
    // (unsigned) -1 - not set.
    unsigned precision;

    enum {
	DefaultNumBase   = 10,
	DefaultMinDigits = 0,
	DefaultPrecision = -1
    };

    Format (unsigned const num_base,
	    unsigned const min_digits,
	    unsigned const precision)
	: num_base (num_base),
	  min_digits (min_digits),
	  precision (precision)
    {
    }

    Format ()
	: num_base (10),
	  min_digits (0),
	  precision ((unsigned) -1)
    {
    }
};

extern Format fmt_def;
extern Format fmt_hex;

// TODO Get rid of this.
static Format const &libMary_default_format = fmt_def;

namespace {

    class FormatFlags
    {
    public:
	enum Value {
	    WithPrecision = 0x1,
	    WithMinDigits = 0x2
	};
	operator Value () const { return value; }
	FormatFlags (Value const value) : value (value) {}
	FormatFlags () {}
    private:
	Value value;
    };

}

template <class T>
Ref<String> toString (T obj, Format const &fmt = libMary_default_format)
{
    Size const len = toString (Memory(), obj, fmt);
    Ref<String> const str = grab (new String (len));
    toString (str->mem(), obj, fmt);
    return str;
}

template <class T>
Size toString (Memory const &mem, T obj, Format const &fmt = libMary_default_format)
{
    return obj.toString_ (mem, fmt);
}

#if 0
// This breaks templates
template <class T>
Size toString (Memory const &mem, T &obj, Format const &fmt = libMary_default_format)
{
    // FIXME This is clearly wrong.
    return obj.toString_ ();
}
#endif

template <class T>
Size toString (Memory const &mem, T * const obj, Format const &fmt = libMary_default_format)
{
    return obj->toString_ (mem, fmt);
}

template <Size N>
inline Size toString (Memory const &mem, char const (&str) [N], Format const & /* fmt */ = libMary_default_format)
{
    Size const len = N - 1;
    if (len <= mem.len())
	memcpy (mem.mem(), str, len);

    return len;
}

template <>
inline Size toString (Memory const &mem, char * const str, Format const & /* fmt */)
{
    Size const len = strlen (str);
    if (len <= mem.len())
	memcpy (mem.mem(), str, len);

    return len;
}

template <>
inline Size toString (Memory const &mem, char const * const str, Format const & /* fmt */)
{
    Size const len = strlen (str);
    if (len <= mem.len())
	memcpy (mem.mem(), str, len);

    return len;
}

template <>
inline Size toString (Memory const &mem, Memory const &str, Format const & /* fmt */)
{
    if (str.len() <= mem.len())
	memcpy (mem.mem(), str.mem(), str.len());

    return str.len();
}

template <>
inline Size toString (Memory const &mem, Memory str, Format const & /* fmt */)
{
    if (str.len() <= mem.len())
	memcpy (mem.mem(), str.mem(), str.len());

    return str.len();
}

template <>
inline Size toString (Memory const &mem, ConstMemory const &str, Format const & /* fmt */)
{
    if (str.len() <= mem.len())
	memcpy (mem.mem(), str.mem(), str.len());

    return str.len();
}

template <>
inline Size toString (Memory const &mem, ConstMemory str, Format const & /* fmt */)
{
    if (str.len() <= mem.len())
	memcpy (mem.mem(), str.mem(), str.len());

    return str.len();
}

// @flags Combination of FormatFlag flags.
template <class T>
inline Size _libMary_snprintf (Memory      const &mem,
			       ConstMemory const &spec_str,
			       T           const value,
			       Format      const &fmt,
			       Uint32      const flags)
{
  // snprintf() is thread-safe according to POSIX.1c

    char format_str [128];
    format_str [0] = '%';
    Size pos = 1;

    if (flags & FormatFlags::WithMinDigits &&
	fmt.min_digits > 0)
    {
	int const res = snprintf (format_str + pos, sizeof (format_str) - pos, ".%u", (unsigned) fmt.min_digits);
	assert (res >= 0);
	assert ((Size) res < sizeof (format_str) - pos);
	pos += res;
    }

    if (flags & FormatFlags::WithPrecision &&
	fmt.precision != (unsigned) -1)
    {
	int const res = snprintf (format_str + pos, sizeof (format_str) - pos, ".%u", (unsigned) fmt.precision);
	assert (res >= 0);
	assert ((Size) res < sizeof (format_str) - pos);
	pos += res;
    }

    assert (sizeof (format_str - pos) > spec_str.len());
    memcpy (format_str + pos, spec_str.mem(), spec_str.len());
    pos += spec_str.len ();

    assert (pos < sizeof (format_str));
    format_str [pos] = 0;

    int const res = snprintf (reinterpret_cast <char*> (mem.mem()), mem.len(), format_str, value);
    assert (res >= 0);

    return res;
}

template <>
inline Size toString (Memory const &mem, char value, Format const &fmt)
{
    if (fmt.num_base == 16)
	return _libMary_snprintf (mem, "x", (unsigned) (unsigned char) value, fmt, FormatFlags::WithMinDigits);

    return _libMary_snprintf (mem, "d", (int) value, fmt, FormatFlags::WithMinDigits);
}

template <>
inline Size toString (Memory const &mem, unsigned char value, Format const &fmt)
{
    if (fmt.num_base == 16)
	return _libMary_snprintf (mem, "x", (unsigned) (unsigned char) value, fmt, FormatFlags::WithMinDigits);

    return _libMary_snprintf (mem, "u", (unsigned) value, fmt, FormatFlags::WithMinDigits);
}

template <>
inline Size toString (Memory const &mem, signed char value, Format const &fmt)
{
    if (fmt.num_base == 16)
	return _libMary_snprintf (mem, "x", (unsigned) (unsigned char) value, fmt, FormatFlags::WithMinDigits);

    return _libMary_snprintf (mem, "d", (int) value, fmt, FormatFlags::WithMinDigits);
}

template <>
inline Size toString (Memory const &mem, short value, Format const &fmt)
{
    if (fmt.num_base == 16)
	return _libMary_snprintf (mem, "x", (unsigned) (unsigned short) value, fmt, FormatFlags::WithMinDigits);

    return _libMary_snprintf (mem, "d", (int) value, fmt, FormatFlags::WithMinDigits);
}

template <>
inline Size toString (Memory const &mem, int value, Format const &fmt)
{
    if (fmt.num_base == 16)
	return _libMary_snprintf (mem, "x", (unsigned) value, fmt, FormatFlags::WithMinDigits);

    return _libMary_snprintf (mem, "d", (int) value, fmt, FormatFlags::WithMinDigits);
}

template <>
inline Size toString (Memory const &mem, long value, Format const &fmt)
{
    if (fmt.num_base == 16)
	return _libMary_snprintf (mem, "lx", (unsigned long) value, fmt, FormatFlags::WithMinDigits);

    return _libMary_snprintf (mem, "ld", (long) value, fmt, FormatFlags::WithMinDigits);
}

template <>
inline Size toString (Memory const &mem, long long value, Format const &fmt)
{
    if (fmt.num_base == 16)
	return _libMary_snprintf (mem, "llx", (unsigned long long) value, fmt, FormatFlags::WithMinDigits);

    return _libMary_snprintf (mem, "lld", (long long) value, fmt, FormatFlags::WithMinDigits);
}

template <>
inline Size toString (Memory const &mem, unsigned short value, Format const &fmt)
{
    if (fmt.num_base == 16)
	return _libMary_snprintf (mem, "x", (unsigned) value, fmt, FormatFlags::WithMinDigits);

    return _libMary_snprintf (mem, "u", (unsigned) value, fmt, FormatFlags::WithMinDigits);
}

template <>
inline Size toString (Memory const &mem, unsigned value, Format const &fmt)
{
    if (fmt.num_base == 16)
	return _libMary_snprintf (mem, "x", (unsigned) value, fmt, FormatFlags::WithMinDigits);

    return _libMary_snprintf (mem, "u", (unsigned) value, fmt, FormatFlags::WithMinDigits);
}

template <>
inline Size toString (Memory const &mem, unsigned long value, Format const &fmt)
{
    if (fmt.num_base == 16)
	return _libMary_snprintf (mem, "lx", (unsigned long) value, fmt, FormatFlags::WithMinDigits);

    return _libMary_snprintf (mem, "lu", (unsigned long) value, fmt, FormatFlags::WithMinDigits);
}

template <>
inline Size toString (Memory const &mem, unsigned long long value, Format const &fmt)
{
    if (fmt.num_base == 16)
	return _libMary_snprintf (mem, "llx", (unsigned long long) value, fmt, FormatFlags::WithMinDigits);

    return _libMary_snprintf (mem, "llu", (unsigned long long) value, fmt, FormatFlags::WithMinDigits);
}

template <>
inline Size toString (Memory const &mem, float value, Format const &fmt)
{
    return _libMary_snprintf (mem, "f", (double) value, fmt, FormatFlags::WithPrecision);
}

template <>
inline Size toString (Memory const &mem, double value, Format const &fmt)
{
    return _libMary_snprintf (mem, "f", (double) value, fmt, FormatFlags::WithPrecision);
}

template <>
inline Size toString (Memory const &mem, long double value, Format const &fmt)
{
    return _libMary_snprintf (mem, "Lf", (long double) value, fmt, FormatFlags::WithPrecision);
}

template <>
inline Size toString (Memory const &mem, bool value, Format const & /* fmt */)
{
    return value ? toString (mem, "true") : toString (mem, "false");
}

static inline
Size _do_measureString (Format const & /* fmt */)
{
    return 0;
}

template <class T, class ...Args>
Size _do_measureString (Format const &fmt,
			T      const &obj,
			Args   const &...args)
{
    Size const sub_len = toString (Memory(), obj, fmt);
    return sub_len + _do_measureString (fmt, args...);
}

template <class ...Args>
Size _do_measureString (Format const & /* fmt */,
			Format const &new_fmt,
			Args   const &...args)
{
    return _do_measureString (new_fmt, args...);
}

template <class ...Args>
Size measureString (Args const &...args)
{
    return _do_measureString (fmt_def, args...);
}

static inline void
_do_makeString (Memory const & /* mem */,
		Format const & /* fmt */)
{
  // No-op
}

template <class T, class ...Args>
void _do_makeString (Memory const &mem,
		     Format const &fmt,
		     T      const &obj,
		     Args   const &...args)
{
    Size const len = toString (mem, obj, fmt);
    assert (len <= mem.len());
    _do_makeString (mem.region (len), fmt, args...);
}

template <class ...Args>
void _do_makeString (Memory const &mem,
		     Format const & /* fmt */,
		     Format const &new_fmt,
		     Args   const &...args)
{
    _do_makeString (mem, new_fmt, args...);
}

template <class ...Args>
Ref<String> makeString (Args const &...args)
{
    Ref<String> const str = grab (new String (measureString (args...)));
    _do_makeString (str->mem(), fmt_def, args...);
    return str;
}

// _____________________________________________________________________________


template <Size N>
ComparisonResult compare (ConstMemory const &left,
			  char const (&right) [N])
{
    Size const tocompare = left.len() <= (N - 1) ? left.len() : (N - 1);
    return (ComparisonResult::Value) memcmp (left.mem(), right, tocompare);
}

ComparisonResult compare (ConstMemory const &left,
			  ConstMemory const &right);

static inline bool equal (ConstMemory const &left,
			  ConstMemory const &right)
{
    if (left.len() != right.len())
	return false;

    return !memcmp (left.mem(), right.mem(), left.len());
}

static inline unsigned long strToUlong (char const * const cstr)
{
    return strtoul (cstr, NULL /* endptr */, 10);
}

unsigned long strToUlong (ConstMemory const &mem);

}


#endif /* __LIBMARY__UTIL_STR_BASE__H__ */

