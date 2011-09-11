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


#ifndef __LIBMARY__DEFERRED_PROCESSOR__H__
#define __LIBMARY__DEFERRED_PROCESSOR__H__


#include <libmary/types.h>
#include <libmary/cb.h>
#include <libmary/intrusive_list.h>


namespace M {

class DeferredProcessor
{
public:
    // Returns 'true' if the task should be rescheduled immediately for another
    // invocation.
    typedef bool TaskCallback (void *cb_data);

    struct Backend
    {
	void (*trigger) (void *cb_data);
    };

    class Registration;
    friend class Registration;

    class TaskList_name;
    class PermanentTaskList_name;
    class RegistrationList_name;
    class PermanentRegistrationList_name;

    class Task : public IntrusiveListElement<TaskList_name>,
		 public IntrusiveListElement<PermanentTaskList_name>
    {
	friend class DeferredProcessor;
	friend class Registration;

    private:
	mt_mutex (DeferredProcessor::mutex) bool scheduled;
	mt_mutex (DeferredProcessor::mutex) bool processing;
	mt_mutex (DeferredProcessor::mutex) bool permanent;

	mt_mutex (DeferredProcessor::mutex) Registration *registration;

    public:
	mt_const Cb<TaskCallback> cb;

	Task ()
	    : scheduled (false),
	      processing (false),
	      permanent (false),
	      registration (NULL)
	{
	}
    };
    typedef IntrusiveList<Task, TaskList_name> TaskList;
    typedef IntrusiveList<Task, PermanentTaskList_name> PermanentTaskList;

    class Registration : public IntrusiveListElement<RegistrationList_name>,
			 public IntrusiveListElement<PermanentRegistrationList_name>
    {
	friend class DeferredProcessor;

    private:
	mt_const DeferredProcessor *deferred_processor;

	mt_mutex (deferred_processor->mutex) TaskList task_list;
	mt_mutex (deferred_processor->mutex) PermanentTaskList permanent_task_list;

	mt_mutex (deferred_processor->mutex) bool scheduled;
	mt_mutex (deferred_processor->mutex) bool permanent_scheduled;

	mt_mutex (deferred_processor->mutex) void rescheduleTask (Task * mt_nonnull task);

    public:
	void scheduleTask (Task * mt_nonnull task,
			   bool  permanent = false);

	void revokeTask (Task * mt_nonnull task);

	mt_const void setDeferredProcessor (DeferredProcessor * const mt_nonnull deferred_processor)
	{
	    this->deferred_processor = deferred_processor;
	}

	void release ();

	Registration ()
	    : deferred_processor (NULL),
	      scheduled (false),
	      permanent_scheduled (false)
	{
	}
    };
    typedef IntrusiveList<Registration, RegistrationList_name> RegistrationList;
    typedef IntrusiveList<Registration, PermanentRegistrationList_name> PermanentRegistrationList;

private:
    Cb<Backend> backend;

    mt_mutex (mutex) RegistrationList registration_list;
    mt_mutex (mutex) PermanentRegistrationList permanent_registration_list;

    mt_mutex (mutex) bool processing;
    mt_mutex (mutex) TaskList processing_task_list;

    Mutex mutex;

public:
    // Returns 'true' if there are more tasks to process.
    bool process ();

    void trigger ();

    mt_const void setBackend (CbDesc<Backend> const &backend)
    {
	this->backend = backend;
    }

    DeferredProcessor ()
	: processing (false)
    {
    }
};

}


#endif /* __LIBMARY__DEFERRED_PROCESSOR__H__ */

