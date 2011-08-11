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


#ifndef __LIBMARY__TYPES_BASE__H__
#define __LIBMARY__TYPES_BASE__H__


// For numeric limits
#include <glib/gtypes.h>
// Using a C header doesn't feel good.
#include <stdint.h>

#if 0
// Another approach to getting numeric limits macros. This one suffers from
// the need for libmary/types_base.h to be the first header used
// in a translation unit.

// Macros like INT8_MAX should be explicitly requested to be defined.
#define __STDC_LIMIT_MACROS
#include <stdint.h>
// This include file is C++0x-specific.
//#include <cstdint>
#endif

#include <cstddef>
#include <cassert>

#include <libmary/annotations.h>


namespace M {

typedef int8_t  Int8;
typedef int16_t Int16;
typedef int32_t Int32;
typedef int64_t Int64;

typedef uint8_t  Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef uint64_t Uint64;

#if 0
// Note that C++ <limits> header is useless, because numeric_limits<>::max/min
// can't be used in constant expressions.
enum {
    Int8_Max  = INT8_MAX,
    Int8_Min  = INT8_MIN,

    Int16_Max = INT16_MAX,
    Int16_Min = INT16_MIN,

    Int32_Max = INT32_MAX,
    Int32_Min = INT32_MIN,

    Int64_Max = INT64_MAX,
    Int64_Min = INT64_MIN
};

enum {
    Uint8_Max  = UINT8_MAX,
    Uint16_Max = UINT16_MAX,
    Uint32_Max = UINT32_MAX,
    Uint64_Max = UINT64_MAX
};
#endif

// Glib macros are more convenient to use because they allow not to abuse
// standard headers.
enum {
    Int_Max = G_MAXINT,

    Int8_Max  = G_MAXINT8,
    Int8_Min  = G_MININT8,

    Int16_Max = G_MAXINT16,
    Int16_Min = G_MININT16,

    Int32_Max = G_MAXINT32,
    Int32_Min = G_MININT32,

    Int64_Max = G_MAXINT64,
    Int64_Min = G_MININT64
};

enum {
    Uint8_Max  = G_MAXUINT8,
    Uint16_Max = G_MAXUINT16,
    Uint32_Max = G_MAXUINT32,
    Uint64_Max = G_MAXUINT64
};

typedef Uint8 Byte;

// TODO Describe when to use what.
typedef size_t    Size;
// Signed offset. Use 'Size' for unsigned offsets.
typedef ptrdiff_t Offset;
typedef uintptr_t UintPtr;
typedef Size      Count;

typedef Int64  FileOffset;
typedef Uint64 FileSize;

// Auto-initialized boolean for porting MyCpp code.
class Bool
{
protected:
    bool value;
public:
    Bool& operator = (bool value) { this->value = value; return *this; }
    operator bool () const { return value; }
    Bool (bool value) : value (value) {}
    Bool () { value = false; }
};

}


#endif /* __LIBMARY__TYPES_BASE__H__ */

