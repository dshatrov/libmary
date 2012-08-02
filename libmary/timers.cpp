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
#include <libmary/util_time.h>

#include <libmary/timers.h>


namespace M {

namespace {
LogGroup libMary_logGroup_timers ("timers", LogLevel::I);
}

Timers::TimerKey
Timers::addTimer_microseconds (CbDesc<TimerCallback> const &cb,
			       Time const time_microseconds,
			       bool const periodical)
{
    logD (timers, _func, "time_microseconds: ", time_microseconds);

    Timer * const timer = new Timer (cb);
    timer->periodical = periodical;
    timer->due_time = getTimeMicroseconds() + time_microseconds;
    if (timer->due_time < time_microseconds) {
	logW_ (_func, "Expiration time overflow");
	timer->due_time = (Time) -1;
    }

    logD (timers, _func, "getTimeMicroseconds(): ", getTimeMicroseconds(), ", due_time: ", timer->due_time);

    mutex.lock ();

    bool first_timer = false;

    TimerChain *chain = interval_tree.lookup (time_microseconds);
    if (!chain) {
	chain = new TimerChain;
	chain->interval_microseconds = time_microseconds;
	chain->nearest_time = timer->due_time;

	if (expiration_tree.isEmpty()
	    || expiration_tree_leftmost->nearest_time > chain->nearest_time)
	{
	    first_timer = true;
	}

	interval_tree.add (chain);
	expiration_tree.add (chain);
	expiration_tree_leftmost = expiration_tree.getLeftmost();
    }

    timer->chain = chain;
    chain->timer_list.append (timer);

    mutex.unlock ();

    if (first_timer) {
	logD (timers, _func, "calling first_added_cb()");
	first_added_cb.call_ ();
    }

    return timer;
}

void
Timers::restartTimer (TimerKey const mt_nonnull timer_key)
{
    Timer * const timer = timer_key;
    TimerChain * const chain = timer->chain;

    mutex.lock ();

    assert (timer->active);

    expiration_tree.remove (chain);

    chain->timer_list.remove (timer);
    timer->due_time = getTimeMicroseconds() + chain->interval_microseconds;
    if (timer->due_time < chain->interval_microseconds) {
	logW_ (_func, "Expiration time overflow");
	timer->due_time = (Time) -1;
    }

    chain->timer_list.append (timer);

    chain->nearest_time = chain->timer_list.getFirst()->due_time;
    expiration_tree.add (chain);
    expiration_tree_leftmost = expiration_tree.getLeftmost();

    mutex.unlock ();
}

void
Timers::deleteTimer (TimerKey const mt_nonnull timer_key)
{
    Timer * const timer = timer_key;
    TimerChain * const chain = timer->chain;

    mutex.lock ();

    if (timer->active) {
	timer->active = false;

	expiration_tree.remove (chain);

	chain->timer_list.remove (timer);
	if (chain->timer_list.isEmpty ()) {
	    expiration_tree_leftmost = expiration_tree.getLeftmost();
	    interval_tree.remove (chain);
	    mutex.unlock ();

	    delete chain;
	    delete timer;

	    return;
	}

	chain->nearest_time = chain->timer_list.getFirst ()->due_time;
	expiration_tree.add (chain);
	expiration_tree_leftmost = expiration_tree.getLeftmost();
    }

    mutex.unlock ();

    delete timer;
}

Time
Timers::getSleepTime_microseconds ()
{
    Time const cur_time = getTimeMicroseconds ();

  MutexLock l (&mutex);

    TimerChain * const chain = expiration_tree_leftmost;
    if (chain == NULL) {
	logD (timers, _func, ": null chain");
	return (Time) -1;
    }

    if (chain->nearest_time <= cur_time) {
	logD (timers, _func, ": now");
	return 0;
    }

    logD (timers, _func, ": nearest: ", chain->nearest_time, ", cur: ", cur_time, ", delta: ", chain->nearest_time - cur_time);

    return chain->nearest_time - cur_time;
}

void
Timers::processTimers ()
{
    Time const cur_time = getTimeMicroseconds ();

    mutex.lock ();

    TimerChain *chain = expiration_tree_leftmost;
    if (chain == NULL) {
	mutex.unlock ();
	return;
    }

    while (!chain->timer_list.isEmpty () && chain->nearest_time <= cur_time) {
	Time const cur_nearest_time = chain->nearest_time;

	logD (timers, _func, "cur_nearest_time: ", cur_nearest_time, ", cur_time: ", cur_time);

	Timer * const timer = chain->timer_list.getFirst ();
	assert (timer->active);
	chain->timer_list.remove (timer);
	if (timer->periodical) {
	    timer->due_time += chain->interval_microseconds;
	    if (timer->due_time < chain->interval_microseconds) {
		logW_ (_func, "Expiration time overflow");
		timer->due_time = (Time) -1;
	    }
	    chain->timer_list.append (timer);
	} else {
	    timer->active = false;
	}

	bool delete_chain;
	if (chain->timer_list.isEmpty ()) {
	    expiration_tree.remove (chain);
	    expiration_tree_leftmost = expiration_tree.getLeftmost();
	    interval_tree.remove (chain);
	    delete_chain = true;
	} else {
	    if (cur_nearest_time != chain->timer_list.getFirst ()->due_time) {
		expiration_tree.remove (chain);
		chain->nearest_time = chain->timer_list.getFirst ()->due_time;
		expiration_tree.add (chain);
		expiration_tree_leftmost = expiration_tree.getLeftmost();
	    }
	    delete_chain = false;
	}

	timer->timer_cb.call_unlocks_mutex_ (mutex);

      // 'timer' might have been deleted by the user and should not be used
      // directly anymore.
      //
      // We can't delete the timer ourselves here for a similar reason: its
      // lifetime is controlled by the user, so we can't tie it to callback's
      // weak_obj.

	if (delete_chain)
	    delete chain;

	mutex.lock ();

	// 'chain' might have been deleted due to user's actions in 'timer_cb' callback.
	chain = expiration_tree_leftmost;
	if (chain == NULL) {
	    mutex.unlock ();
	    return;
	}
    }

    mutex.unlock ();
}

Timers::Timers ()
    : expiration_tree_leftmost (NULL)
{
}

Timers::Timers (FirstTimerAddedCallback * const cb,
		void                    * const cb_data,
		Object                  * const coderef_container)
    : expiration_tree_leftmost (NULL),
      first_added_cb (cb, cb_data, coderef_container)
{
}

Timers::~Timers ()
{
    mutex.lock ();

    {
        ChainCleanupList chain_list;
        {
            IntervalTree::Iterator chain_iter (interval_tree);
            while (!chain_iter.done ()) {
                TimerChain * const chain = chain_iter.next ();
                chain_list.append (chain);
            }
        }

        ChainCleanupList::iter chain_iter (chain_list);
        while (!chain_list.iter_done (chain_iter)) {
            TimerChain * const chain = chain_list.iter_next (chain_iter);

            IntrusiveList<Timer>::iter timer_iter (chain->timer_list);
            while (!chain->timer_list.iter_done (timer_iter)) {
                Timer * const timer = chain->timer_list.iter_next (timer_iter);
                assert (timer->active);
                delete timer;
            }

            delete chain;
        }
    }

    mutex.unlock ();
}

}

