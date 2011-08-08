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


#include "local_config.h"

#include <libmary/log.h>


namespace M {

char const _libMary_loglevel_str_A [5] = "] A ";
char const _libMary_loglevel_str_D [5] = "] D ";
char const _libMary_loglevel_str_I [5] = "] I ";
char const _libMary_loglevel_str_W [5] = "] W ";
char const _libMary_loglevel_str_E [5] = "] E ";
char const _libMary_loglevel_str_H [5] = "] H ";
char const _libMary_loglevel_str_F [5] = "] F ";
char const _libMary_loglevel_str_N [5] = "] N ";

LogGroup libMary_logGroup_default ("default", LogLevel::All);

Mutex _libMary_log_mutex;

LogGroup::LogGroup (ConstMemory const &group_name,
		    unsigned    const loglevel)
    : loglevel (loglevel)
{
    // TODO Add the group to global hash.
}

}

