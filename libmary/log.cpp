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


#include <libmary/log.h>


namespace M {

char const _libMary_loglevel_str_D [4] = " D ";
char const _libMary_loglevel_str_I [4] = " I ";
char const _libMary_loglevel_str_W [4] = " W ";
char const _libMary_loglevel_str_A [9] = " ACCESS ";
char const _libMary_loglevel_str_E [4] = " E ";
char const _libMary_loglevel_str_H [4] = " H ";
char const _libMary_loglevel_str_F [4] = " F ";
char const _libMary_loglevel_str_N [4] = " N ";

LogGroup libMary_logGroup_default ("default", LogLevel::All);
LogLevel libMary_globalLogLevel (LogLevel::All);

Mutex _libMary_log_mutex;

char const * LogLevel::toCompactCstr ()
{
    switch (value) {
	case LogLevel::All:
	    return "ALL";
	case LogLevel::Debug:
	    return "D";
	case LogLevel::Info:
	    return "I";
	case LogLevel::Warning:
	    return "W";
	case LogLevel::Access:
	    return "ACCESS";
	case LogLevel::Error:
	    return "E";
	case LogLevel::High:
	    return "H";
	case LogLevel::Failure:
	    return "F";
	case LogLevel::None:
	    return "N";
	default:
	    unreachable ();
    }

    // unreachable
    return "";
}

Result
LogLevel::fromString (ConstMemory   const str,
		      LogLevel    * const mt_nonnull ret_loglevel)
{
    LogLevel loglevel = LogLevel::Info;

    if (equal (str, "All")) {
	loglevel = LogLevel::All;
    } else
    if (equal (str, "D") || equal (str, "Debug")) {
	loglevel = LogLevel::Debug;
    } else
    if (equal (str, "I") || equal (str, "Info")) {
	loglevel = LogLevel::Info;
    } else
    if (equal (str, "W") || equal (str, "Warning")) {
	loglevel = LogLevel::Warning;
    } else
    if (equal (str, "A") || equal (str, "Access")) {
	loglevel = LogLevel::Access;
    } else
    if (equal (str, "E") || equal (str, "Error")) {
	loglevel = LogLevel::Error;
    } else
    if (equal (str, "H") || equal (str, "High")) {
	loglevel = LogLevel::High;
    } else
    if (equal (str, "F") || equal (str, "Failure")) {
	loglevel = LogLevel::Failure;
    } else
    if (equal (str, "N") || equal (str, "None")) {
	loglevel = LogLevel::None;
    } else {
	*ret_loglevel = LogLevel::Info;
	return Result::Failure;
    }

    *ret_loglevel = loglevel;
    return Result::Success;
}

LogGroup::LogGroup (ConstMemory const &group_name,
		    unsigned    const loglevel)
    : loglevel (loglevel)
{
    // TODO Add the group to global hash.
}

}

