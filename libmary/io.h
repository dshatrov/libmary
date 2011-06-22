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


#ifndef __LIBMARY__IO__H__
#define __LIBMARY__IO__H__


//#include <libmary/file.h>
#include <libmary/output_stream.h>


namespace M {

//extern File *outf;
//extern File *errf;

extern OutputStream *outs;
extern OutputStream *errs;

//#error Log streams
//extern OutputStream 

}


#endif /* __LIBMARY__IO__H__ */

