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


#include <libmary/libmary_config.h>

#include <libmary/types.h>
#ifdef LIBMARY_MT_SAFE
#include <glib/gthread.h>
#endif


namespace M {

/*c */
class Mutex
{
private:
#ifdef LIBMARY_MT_SAFE
    GStaticMutex mutex;
#endif

public:
    /*m Locks the mutex. */
    void lock ()
    {
#ifdef LIBMARY_MT_SAFE
	g_static_mutex_lock (&mutex);
#endif
    }

    /*m Unlocks the mutex. */
    void unlock ()
    {
#ifdef LIBMARY_MT_SAFE
	g_static_mutex_unlock (&mutex);
#endif
    }

#ifdef LIBMARY_MT_SAFE
    /* For internal use only:
     * should not be expected to be present in future versions. */
    GMutex* get_glib_mutex ()
    {
	return g_static_mutex_get_mutex (&mutex);
    }
#endif

#ifdef LIBMARY_MT_SAFE
    Mutex ()
    {
	g_static_mutex_init (&mutex);
    }
#endif

#ifdef LIBMARY_MT_SAFE
    ~Mutex ()
    {
	g_static_mutex_free (&mutex);
    }
#endif
};

class MutexLock
{
private:
#ifdef LIBMARY_MT_SAFE
    Mutex * const mutex;
#endif

    MutexLock& operator = (MutexLock const &);
    MutexLock (MutexLock const &);

public:
#ifdef LIBMARY_MT_SAFE
    MutexLock (Mutex * const mutex)
	: mutex (mutex)
    {
	mutex->lock ();
    }

    MutexLock (Mutex &mutex)
	: mutex (&mutex)
    {
	mutex.lock ();
    }
#else
    MutexLock (Mutex * const /* mutex */)
    {
    }
#endif

#ifdef LIBMARY_MT_SAFE
    ~MutexLock ()
    {
	mutex->unlock ();
    }
#endif
};

class MutexUnlock
{
private:
#ifdef LIBMARY_MT_SAFE
    Mutex * const mutex;
#endif

    MutexUnlock& operator = (MutexUnlock const &);
    MutexUnlock (MutexUnlock const &);

public:
#ifdef LIBMARY_MT_SAFE
    MutexUnlock (Mutex * const mutex)
	: mutex (mutex)
    {
	mutex->unlock ();
    }

    MutexUnlock (Mutex &mutex)
	: mutex (&mutex)
    {
	mutex.unlock ();
    }
#else
    MutexUnlock (Mutex * const /* mutex */)
    {
    }
#endif

#ifdef LIBMARY_MT_SAFE
    ~MutexUnlock ()
    {
	mutex->lock ();
    }
#endif
};

}


#endif /* __LIBMARY__MUTEX__H__ */

