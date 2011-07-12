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
    class Registration;
    friend class Registration;

    class TaskList_name;
    class ProcessingTaskList_name;
    class RegistrationList_name;

    class Task : public IntrusiveListElement<TaskList_name>,
		 public IntrusiveListElement<ProcessingTaskList_name>
    {
	friend class DeferredProcessor;
	friend class Registration;

    private:
	mt_mutex (DeferredProcessor::mutex) bool scheduled;
	mt_mutex (DeferredProcessor::mutex) bool processing;

    public:
	mt_const Cb<GenericCallback> cb;

	Task ()
	    : scheduled (false),
	      processing (false)
	{
	}
    };
    typedef IntrusiveList<Task, TaskList_name> TaskList;
    typedef IntrusiveList<Task, ProcessingTaskList_name> ProcessingTaskList;

    class Registration : public IntrusiveListElement<RegistrationList_name>
    {
	friend class DeferredProcessor;

    private:
	mt_const DeferredProcessor *deferred_processor;

	mt_mutex (DeferredProcessor::mutex) TaskList task_list;

	mt_mutex (DeferredProcessor::mutex) bool scheduled;

    public:
	void scheduleTask (Task * mt_nonnull task);

	void revokeTask (Task * mt_nonnull task);

	mt_const void setDeferredProcessor (DeferredProcessor * const mt_nonnull deferred_processor)
	{
	    this->deferred_processor = deferred_processor;
	}

	void release ();

	Registration ()
	    : deferred_processor (NULL),
	      scheduled (false)
	{
	}
    };
    typedef IntrusiveList<Registration, RegistrationList_name> RegistrationList;

private:
    mt_mutex (mutex) RegistrationList registration_list;

    bool processing;
    mt_mutex (mutex) ProcessingTaskList processing_task_list;

    StateMutex mutex;

public:
    // Returns 'true' if there are more tasks to process.
    bool process ();

    DeferredProcessor ()
	: processing (false)
    {
    }
};

}


#endif /* __LIBMARY__DEFERRED_PROCESSOR__H__ */

