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


#include <libmary/deferred_processor.h>


namespace M {

void
DeferredProcessor::Registration::scheduleTask (Task * const mt_nonnull task)
{
    deferred_processor->mutex.lock ();

    if (task->scheduled ||
	task->processing)
    {
	deferred_processor->mutex.unlock ();
	return;
    }

    task_list.append (task);
    task->scheduled = true;

    if (scheduled) {
	deferred_processor->mutex.unlock ();
	return;
    }
    deferred_processor->registration_list.append (this);
    scheduled = true;

    deferred_processor->mutex.unlock ();
}

void
DeferredProcessor::Registration::revokeTask (Task * const mt_nonnull task)
{
    deferred_processor->mutex.lock ();

    if (task->processing) {
	assert (!task->scheduled);
	deferred_processor->processing_task_list.remove (task);
	task->processing = false;
    } else
    if (task->scheduled) {
	task_list.remove (task);
	task->scheduled = false;
    }

    if (task_list.isEmpty()) {
	deferred_processor->registration_list.remove (this);
	scheduled = false;
    }

    deferred_processor->mutex.unlock ();
}

bool
DeferredProcessor::process ()
{
    mutex.lock ();

    assert (!processing);
    processing = true;

    {
	RegistrationList::iter reg_iter (registration_list);
	while (!registration_list.iter_done (reg_iter)) {
	    Registration * const reg = registration_list.iter_next (reg_iter);

	    TaskList::iter task_iter (reg->task_list);
	    while (!reg->task_list.iter_done (task_iter)) {
		Task * const task = reg->task_list.iter_next (task_iter);
		task->processing = true;
	    }

	    processing_task_list.stealAppend (reg->task_list.getFirst(), reg->task_list.getLast());
	    reg->task_list.clear ();
	}
    }

    {
	ProcessingTaskList::iter iter (processing_task_list);
	while (!processing_task_list.iter_done (iter)) {
	    Task * const task = processing_task_list.iter_next (iter);
	    if (!task)
		break;

	    processing_task_list.remove (task);
	    task->processing = false;

	    task->cb.call_mutex_ (mutex);
	}
    }

    processing = false;

    if (registration_list.isEmpty()) {
	mutex.unlock ();
	return false;
    }

    mutex.unlock ();
    return true;
}

}

