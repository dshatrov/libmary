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


#ifndef __LIBMARY__TIMERS__H__
#define __LIBMARY__TIMERS__H__


#include <libmary/types.h>
#include <libmary/intrusive_list.h>
#include <libmary/avl_tree.h>
#include <libmary/cb.h>
#include <libmary/mutex.h>
#include <libmary/util_time.h>

namespace M {

// Алгоритм работы таймеров:
// 1. Таймеры группируются по периоду срабатывания;
// 2. Группы таймеров сортируются AVL-деревом;
// 3. После срабатывания таймера переставляем группу по следующему таймеру
//    с тем же интервалом;
// 4. Два типа таймеров: одноразовые и периодические.
//
// Таймеры должны быть MT-safe.

class Timers
{
private:
    class Timer;
    class TimerChain;

public:
    typedef Timer* TimerKey;

    typedef void (TimerCallback) (void *cb_data);

    typedef void (FirstTimerAddedCallback) (void *cb_data);

private:
    class Timer : public IntrusiveListElement<>
    {
    public:
	mt_const bool periodical;
	mt_const Cb<TimerCallback> timer_cb;
	mt_const TimerChain *chain;

	Time due_time;

	Timer (TimerCallback * const cb,
	       void          * const cb_data,
	       Object        * const coderef_container)
	    : timer_cb (cb, cb_data, coderef_container)
	{
	}
    };

    // A chain of timers with the same expiration interval.
    class TimerChain
    {
    public:
	mt_const Time interval_microseconds;
	IntrusiveList<Timer> timer_list;
	// Nearest expiration time.
	Time nearest_time;

	// TODO IntrusiveAvlTree
	typedef AvlTree< TimerChain*,
			 MemberExtractor< TimerChain const,
					  Time const,
					  &TimerChain::interval_microseconds >,
			 DirectComparator<Time> >
		IntervalTree_;

	// TODO IntrusiveAvlTree
	typedef AvlTree< TimerChain*,
			 MemberExtractor< TimerChain const,
					  Time const,
					  &TimerChain::nearest_time >,
			 DirectComparator<Time> >
		ExpirationTree_;

	mt_const IntervalTree_::Node *interval_tree_node;
	ExpirationTree_::Node *expiration_tree_node;
    };

    typedef TimerChain::IntervalTree_ IntervalTree;
    typedef TimerChain::ExpirationTree_ ExpirationTree;

    // Chains sorted by interval_microseconds.
    IntervalTree interval_tree;
    // Chains sorted by nearest_time.
    ExpirationTree expiration_tree;

    mt_const Cb<FirstTimerAddedCallback> first_added_cb;

    Mutex mutex;

public:
    TimerKey addTimer (TimerCallback * const cb,
		       void          * const cb_data,
		       Object        * const coderef_container,
		       Time            const time_seconds,
		       bool            const periodical = false)
    {
	return addTimer_microseconds (cb, cb_data, coderef_container, time_seconds * 1000000, periodical);
    }

    TimerKey addTimer_microseconds (TimerCallback *cb,
				    void          *cb_data,
				    Object        *coderef_container,
				    Time           time_microseconds,
				    bool           periodical = false);

    void restartTimer (TimerKey timer_key);

    void deleteTimer (TimerKey timer_key);

    Time getSleepTime_microseconds ();

    void processTimers ();

    Timers ()
    {
    }

    // @cb is called whenever a new timer appears at the head of timer chain,
    // i.e. when the nearest expiration time changes.
    Timers (FirstTimerAddedCallback * const cb,
	    void                    * const cb_data,
	    Object                  * const coderef_container)
	: first_added_cb (cb, cb_data, coderef_container)
    {
    }

    ~Timers ();
};

}


#endif /* __LIBMARY__TIMERS__H__ */

