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


#ifndef __LIBMARY__STATE_MUTEX__H__
#define __LIBMARY__STATE_MUTEX__H__


#include <libmary/libmary_config.h>
#include <libmary/mutex.h>


namespace M {

class StateMutex
{
private:
#ifdef LIBMARY_MT_SAFE
    Mutex mutex;
#endif

public:
    void lock ();

    void unlock ();

#ifdef LIBMARY_MT_SAFE
    /* For internal use only:
     * should not be expected to be present in future versions. */
    GMutex* get_glib_mutex ()
    {
	return mutex.get_glib_mutex();
    }
#endif
};

class StateMutexLock
{
private:
#ifdef LIBMARY_MT_SAFE
    StateMutex * const mutex;
#endif

    StateMutexLock& operator = (StateMutexLock const &);
    StateMutexLock (StateMutexLock const &);

public:
#ifdef LIBMARY_MT_SAFE
    StateMutexLock (StateMutex * const mutex)
	: mutex (mutex)
    {
	mutex->lock ();
    }

    StateMutexLock (StateMutex &mutex)
	: mutex (&mutex)
    {
	mutex.lock ();
    }
#else
    StateMutexLock (StateMutex * const /* mutex */)
    {
    }
#endif

#ifdef LIBMARY_MT_SAFE
    ~StateMutexLock ()
    {
	mutex->unlock ();
    }
#endif
};

class StateMutexUnlock
{
private:
#ifdef LIBMARY_MT_SAFE
    StateMutex * const mutex;
#endif

    StateMutexUnlock& operator = (StateMutexUnlock const &);
    StateMutexUnlock (StateMutexUnlock const &);

public:
#ifdef LIBMARY_MT_SAFE
    StateMutexUnlock (StateMutex * const mutex)
	: mutex (mutex)
    {
	mutex->unlock ();
    }

    StateMutexUnlock (StateMutex &mutex)
	: mutex (&mutex)
    {
	mutex.unlock ();
    }
#else
    StateMutexUnlock (StateMutex * const /* mutex */)
    {
    }
#endif

#ifdef LIBMARY_MT_SAFE
    ~StateMutexUnlock ()
    {
	mutex->lock ();
    }
#endif
};

}


#endif /* __LIBMARY__STATE_MUTEX__H__ */

