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
LogGroup libMary_logGroup_timers ("timers", LogLevel::N);
}

Timers::TimerKey
Timers::addTimer_microseconds (TimerCallback * const cb,
			       void          * const cb_data,
			       Object        * const coderef_container,
			       Time            const time_microseconds,
			       bool            const periodical)
{
    Timer * const timer = new Timer (cb, cb_data, coderef_container);
    timer->periodical = periodical;
    timer->due_time = getTimeMicroseconds() + time_microseconds;

    logD (timers, _func, "getTimeMicroseconds(): ", getTimeMicroseconds(), ", due_time: ", timer->due_time);

    mutex.lock ();

    bool first_timer = false;

    // TODO AvlTree::addUniqueFor()
    IntervalTree::Node * const chain_node = interval_tree.lookup (time_microseconds);
    TimerChain *chain;
    if (!chain_node) {
	chain = new TimerChain;
	chain->interval_microseconds = time_microseconds;
	chain->nearest_time = timer->due_time;

	if (expiration_tree.isEmpty()
	    || expiration_tree.getLeftmost()->value->nearest_time < chain->nearest_time)
	{
	    first_timer = true;
	}

	chain->interval_tree_node = interval_tree.add (chain);
	chain->expiration_tree_node = expiration_tree.add (chain);
    } else {
	chain = chain_node->value;
    }

    timer->chain = chain;
    chain->timer_list.append (timer);

    mutex.unlock ();

    if (first_timer)
	first_added_cb.call_ ();

    return timer;
}

void
Timers::restartTimer (TimerKey const timer_key)
{
    Timer * const timer = timer_key;
    TimerChain * const chain = timer->chain;

    mutex.lock ();

    expiration_tree.remove (chain->expiration_tree_node);

    chain->timer_list.remove (timer);
    timer->due_time = getTimeMicroseconds() + chain->interval_microseconds;

    chain->timer_list.append (timer);

    chain->nearest_time = chain->timer_list.getFirst()->due_time;
    chain->expiration_tree_node = expiration_tree.add (chain);

    mutex.unlock ();
}

void
Timers::deleteTimer (TimerKey const timer_key)
{
    Timer * const timer = timer_key;
    TimerChain * const chain = timer->chain;

    mutex.lock ();

    expiration_tree.remove (chain->expiration_tree_node);

    chain->timer_list.remove (timer);
    if (chain->timer_list.isEmpty ()) {
	interval_tree.remove (chain->interval_tree_node);
	mutex.unlock ();

	delete chain;
	delete timer;

	return;
    }

    chain->nearest_time = chain->timer_list.getFirst ()->due_time;
    chain->expiration_tree_node = expiration_tree.add (chain);

    mutex.unlock ();

    delete timer;
}

Time
Timers::getSleepTime_microseconds ()
{
    Time const cur_time = getTimeMicroseconds ();

  MutexLock l (&mutex);

    ExpirationTree::Node * const chain_node = expiration_tree.getLeftmost();
    if (chain_node == NULL)
	return (Time) -1;
    TimerChain * const chain = chain_node->value;
    if (chain == NULL)
	return (Time) -1;

    if (chain->nearest_time <= cur_time)
	return 0;

//    logD_ (_func, ": nearest: ", chain->nearest_time, ", cur: ", cur_time, ", delta: ", chain->nearest_time - cur_time);

    return chain->nearest_time - cur_time;
}

void
Timers::processTimers ()
{
    Time const cur_time = getTimeMicroseconds ();

    mutex.lock ();

    ExpirationTree::Node * const chain_node = expiration_tree.getLeftmost();
    if (chain_node == NULL) {
	mutex.unlock ();
	return;
    }
    TimerChain * const chain = chain_node->value;
    if (chain == NULL) {
	mutex.unlock ();
	return;
    }

    while (!chain->timer_list.isEmpty () && chain->nearest_time <= cur_time) {
	Time const cur_nearest_time = chain->nearest_time;

	logD (timers, _func, "cur_nearest_time: ", cur_nearest_time, ", cur_time: ", cur_time);

	Timer * const timer = chain->timer_list.getFirst ();
	chain->timer_list.remove (timer);
	if (timer->periodical) {
	    timer->due_time += chain->interval_microseconds;
	    chain->timer_list.append (timer);
	}

	bool delete_chain;
	if (chain->timer_list.isEmpty ()) {
	    expiration_tree.remove (chain->expiration_tree_node);
	    interval_tree.remove (chain->interval_tree_node);
	    delete_chain = true;
	} else {
	    if (cur_nearest_time != chain->timer_list.getFirst ()->due_time) {
		expiration_tree.remove (chain->expiration_tree_node);
		chain->nearest_time = chain->timer_list.getFirst ()->due_time;
		chain->expiration_tree_node = expiration_tree.add (chain);
	    }

	    delete_chain = false;
	}
	mutex.unlock ();

	if (delete_chain)
	    delete chain;

      // TODO This place is not MT-safe.

	// Non-periodical timers are deleted automatically without user's
	// intervention.
	bool const delete_timer = !timer->periodical;
	timer->timer_cb.call_ ();

      // 'timer' might have been deleted by the user and should not be used
      // directly anymore.

	if (delete_timer)
	    delete timer;

	if (delete_chain)
	    return;

	mutex.lock ();
    }

    mutex.unlock ();
}

Timers::~Timers ()
{
    mutex.lock ();

    IntervalTree::Iterator chain_iter (interval_tree);
    while (!chain_iter.done ()) {
	TimerChain * const chain = chain_iter.next ().value;

	IntrusiveList<Timer>::iter timer_iter (chain->timer_list);
	while (!chain->timer_list.iter_done (timer_iter)) {
	    Timer * const timer = chain->timer_list.iter_next (timer_iter);
	    delete timer;
	}

	delete chain;
    }

    mutex.unlock ();
}

}

