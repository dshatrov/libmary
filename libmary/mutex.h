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


#ifndef __LIBMARY__MUTEX__H__
#define __LIBMARY__MUTEX__H__


#include <glib/gthread.h>


namespace M {

/*c */
class Mutex
{
private:
    GStaticMutex mutex;

public:
    /*m Locks the mutex. */
    void lock ()
    {
	g_static_mutex_lock (&mutex);
    }

    /*m Unlocks the mutex. */
    void unlock ()
    {
	g_static_mutex_unlock (&mutex);
    }

    /* For internal use only:
     * should not be expected to be present in future versions. */
    GMutex* get_glib_mutex ()
    {
	return g_static_mutex_get_mutex (&mutex);
    }

    Mutex ()
    {
	g_static_mutex_init (&mutex);
    }

    ~Mutex ()
    {
	g_static_mutex_free (&mutex);
    }
};

class MutexLock
{
private:
    Mutex * const mutex;

    MutexLock& operator = (MutexLock const &);
    MutexLock (MutexLock const &);

public:
    MutexLock (Mutex * const mutex)
	: mutex (mutex)
    {
	mutex->lock ();
    }

    ~MutexLock ()
    {
	mutex->unlock ();
    }
};

class MutexUnlock
{
private:
    Mutex * const mutex;

    MutexUnlock& operator = (MutexUnlock const &);
    MutexUnlock (MutexUnlock const &);

public:
    MutexUnlock (Mutex * const mutex)
	: mutex (mutex)
    {
	mutex->unlock ();
    }

    ~MutexUnlock ()
    {
	mutex->lock ();
    }
};

}


#endif /* __LIBMARY__MUTEX__H__ */

