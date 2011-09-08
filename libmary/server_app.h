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


#ifndef __LIBMARY__SERVER_APP__H__
#define __LIBMARY__SERVER_APP__H__


#include <libmary/libmary_config.h>

#include <libmary/types.h>
#include <libmary/code_referenced.h>
#include <libmary/exception.h>
#include <libmary/timers.h>
#include <libmary/active_poll_group.h>
//#include <libmary/deferred_processor.h>

#ifdef LIBMARY_MT_SAFE
#include <libmary/multi_thread.h>
#endif


namespace M {

class ServerApp : public DependentCodeReferenced
{
private:
#ifdef LIBMARY_MT_SAFE
    class AppPollGroup : public PollGroup
    {
    private:
	class PollableReg : public IntrusiveListElement<>
	{
	public:
	    PollGroup *poll_group;
	    PollGroup::PollableKey pollable_key;
	};

	typedef IntrusiveList<PollableReg> PollableRegList;

	mt_const ServerApp * const server_app;

	mt_mutex (server_app->mutex) PollableRegList pollable_reg_list;

    public:
	mt_throws PollGroup::PollableKey addPollable (CbDesc<Pollable> const &pollable,
						      DeferredProcessor::Registration *ret_reg);

	void removePollable (PollGroup::PollableKey mt_nonnull key);

	AppPollGroup (ServerApp * const server_app)
	    : server_app (server_app)
	{
	}

	~AppPollGroup ();
    };

    class ThreadData : public Object
    {
    public:
	DefaultPollGroup poll_group;
	DeferredProcessor deferred_processor;

	ThreadData ()
	    : poll_group (this /* coderef_container */)
	{
	}
    };
#endif

    Timers timers;
    DefaultPollGroup poll_group;
//    DeferredProcessor deferred_processor;

#ifdef LIBMARY_MT_SAFE
    AppPollGroup app_poll_group;

    mt_const Ref<MultiThread> multi_thread;

    typedef List< Ref<ThreadData> > ThreadDataList;

    mt_mutex (mutex) ThreadDataList thread_data_list;

    mt_mutex (mutex) ThreadDataList::Element *thread_selector;
#endif

    AtomicInt should_stop;

    void doTimerIteration ();

    static void firstTimerAdded (void *_self);

    static void threadFunc (void *_self);

  mt_iface (ActivePollGroup::Frontend)

    static ActivePollGroup::Frontend poll_frontend;

    static void pollIterationBegin (void *_self);

    static bool pollIterationEnd (void *_self);

  mt_iface_end()

#ifdef LIBMARY_MT_SAFE
    StateMutex mutex;
#endif

public:
    Timers* getTimers ()
    {
	return &timers;
    }

    PollGroup* getPollGroup ()
    {
#ifdef LIBMARY_MT_SAFE
	return &app_poll_group;
#else
	return &poll_group;
#endif
    }

    // TEST For testing purposes only.
    ActivePollGroup* getActivePollGroup ()
    {
	return &poll_group;
    }

    mt_throws Result init ();

    mt_throws Result run ();

    void stop ();

    // Should be called before run().
    mt_const void setNumThreads (Count const num_threads)
    {
#ifdef LIBMARY_MT_SAFE
	multi_thread->setNumThreads (num_threads);
#else
	(void) num_threads;
#endif
    }

    ServerApp (Object *coderef_container,
	       Count   num_threads = 0);
};

}


#endif /* __LIBMARY__SERVER_APP__H__ */

