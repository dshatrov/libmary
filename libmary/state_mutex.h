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


#include <libmary/mutex.h>


namespace M {

class StateMutex
{
private:
    Mutex mutex;

public:
    void lock ();

    void unlock ();

    // TODO get_glib_mutex for Cond
};

class StateMutexLock
{
private:
    StateMutex * const mutex;

    StateMutexLock& operator = (StateMutexLock const &);
    StateMutexLock (StateMutexLock const &);

public:
    StateMutexLock (StateMutex * const mutex)
	: mutex (mutex)
    {
	mutex->lock ();
    }

    ~StateMutexLock ()
    {
	mutex->unlock ();
    }
};

class StateMutexUnlock
{
private:
    StateMutex * const mutex;

    StateMutexUnlock& operator = (StateMutexUnlock const &);
    StateMutexUnlock (StateMutexUnlock const &);

public:
    StateMutexUnlock (StateMutex * const mutex)
	: mutex (mutex)
    {
	mutex->unlock ();
    }

    ~StateMutexUnlock ()
    {
	mutex->lock ();
    }
};

}


#endif /* __LIBMARY__STATE_MUTEX__H__ */

