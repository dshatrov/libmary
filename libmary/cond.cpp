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
#include <glib/gthread.h>

#include <libmary/cond.h>


namespace M {

void
Cond::signal ()
{
    g_cond_signal (static_cast <GCond*> (cond));
}

void
Cond::wait (Mutex &mutex)
{
    g_cond_wait (static_cast <GCond*> (cond), mutex.get_glib_mutex());
}

void
Cond::wait (StateMutex &mutex)
{
    g_cond_wait (static_cast <GCond*> (cond), mutex.get_glib_mutex());
}

Cond::Cond ()
{
    cond = static_cast <void*> (g_cond_new ());
}

Cond::~Cond ()
{
    g_cond_free (static_cast <GCond*> (cond));
}

}

