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


#include <libmary/libmary_config.h>
#include <libmary/deferred_connection_sender.h>
#include <libmary/util_time.h>
#include <libmary/log.h>

#ifdef LIBMARY_ENABLE_MWRITEV
#include <libmary/mwritev.h>
#endif

#include <libmary/server_app.h>


namespace M {

namespace {
LogGroup libMary_logGroup_server_app ("server_app", LogLevel::N);
}

ActivePollGroup::Frontend ServerApp::poll_frontend = {
    pollIterationBegin,
    pollIterationEnd
};

#ifdef LIBMARY_MT_SAFE
mt_throws PollGroup::PollableKey
ServerApp::AppPollGroup::addPollable (CbDesc<PollGroup::Pollable> const &pollable,
				      DeferredProcessor::Registration * const ret_reg)
{
    PollGroup *poll_group;

    server_app->mutex.lock ();
    if (server_app->thread_selector) {
	poll_group = &server_app->thread_selector->data->poll_group;
	server_app->thread_selector = server_app->thread_selector->next;
    } else {
	if (!server_app->thread_data_list.isEmpty ()) {
	    poll_group = &server_app->thread_data_list.getFirst()->poll_group;
	    server_app->thread_selector = server_app->thread_data_list.getFirstElement()->next;
	} else {
	    poll_group = &server_app->poll_group;
	}
    }
    server_app->mutex.unlock ();

    PollGroup::PollableKey const pollable_key = poll_group->addPollable (pollable, ret_reg);
    if (!pollable_key)
	return NULL;

    PollableReg * const pollable_reg = new PollableReg;
    pollable_reg->poll_group = poll_group;
    pollable_reg->pollable_key = pollable_key;

    server_app->mutex.lock ();
    pollable_reg_list.append (pollable_reg);
    server_app->mutex.unlock ();

    return static_cast <PollGroup::PollableKey> (pollable_reg);
}

mt_throws void
ServerApp::AppPollGroup::removePollable (PollableKey const mt_nonnull key)
{
    PollableReg * const pollable_reg = static_cast <PollableReg*> (key);

    pollable_reg->poll_group->removePollable (pollable_reg->pollable_key);

    server_app->mutex.lock ();
    pollable_reg_list.remove (pollable_reg);
    server_app->mutex.unlock ();
}

ServerApp::AppPollGroup::~AppPollGroup ()
{
    server_app->mutex.lock ();

    {
	PollableRegList::iter iter (pollable_reg_list);
	while (!pollable_reg_list.iter_done (iter)) {
	    PollableReg * const pollable_reg = pollable_reg_list.iter_next (iter);
	    delete pollable_reg;
	}
    }

    server_app->mutex.unlock ();
}
#endif // LIBMARY_MT_SAFE

void
ServerApp::doTimerIteration ()
{
    if (!updateTime ())
	logE_ (_func, exc->toString());

    timers.processTimers ();
}

void
ServerApp::firstTimerAdded (void * const _self)
{
    logD (server_app, _func_);
    ServerApp * const self = static_cast <ServerApp*> (_self);
    self->poll_group.trigger ();
}

void
ServerApp::pollIterationBegin (void * const _self)
{
    ServerApp * const self = static_cast <ServerApp*> (_self);
    self->doTimerIteration ();
}

bool
ServerApp::pollIterationEnd (void * const /* _self */)
{
//    logD_ (_func_);
#ifdef LIBMARY_ENABLE_MWRITEV
    if (libMary_mwritevAvailable())
	return DeferredConnectionSender::pollIterationEnd_mwritev ();
#endif
    return DeferredConnectionSender::pollIterationEnd ();
}

#if 0
bool
ServerApp::pollIterationEnd (void *  const _deferred_processor)
{
    DeferredProcessor * const deferred_processor = static_cast <DeferredProcessor*> (_deferred_processor);

    return deferred_processor->process ();
}
#endif

mt_throws Result
ServerApp::init ()
{
    if (!poll_group.open ())
	return Result::Failure;

    poll_group.setFrontend (Cb<ActivePollGroup::Frontend> (&poll_frontend, this, getCoderefContainer()));

    return Result::Success;
}

void
ServerApp::threadFunc (void * const _self)
{
    ServerApp * const self = static_cast <ServerApp*> (_self);

    Ref<ThreadData> const thread_data = grab (new ThreadData);

    if (!thread_data->poll_group.open ()) {
	logE_ (_func, "poll_group.open() failed: ", exc->toString());
	return;
    }

    self->mutex.lock ();
    if (self->should_stop.get()) {
	self->mutex.unlock ();
	return;
    }

    self->thread_data_list.append (thread_data);
    self->mutex.unlock ();

    for (;;) {
	if (!thread_data->poll_group.poll ()) {
	    logE_ (_func, "poll_group.poll() failed: ", exc->toString());
	    // TODO This is a fatal error, but we should exit gracefully nevertheless.
	    abort ();
	    break;
	}

	if (self->should_stop.get())
	    break;
    }
}

mt_throws Result
ServerApp::run ()
{
    if (!multi_thread->spawn (true /* joinable */)) {
	logE_ (_func, "multi_thread->spawn() failed: ", exc->toString());
	return Result::Failure;
    }

    for (;;) {
	if (!poll_group.poll (timers.getSleepTime_microseconds())) {
	    logE_ (_func, "poll_group.poll() failed: ", exc->toString());
	    stop ();
	    multi_thread->join ();
	    return Result::Failure;
	}

	if (should_stop.get())
	    break;
    }

    stop ();
    multi_thread->join ();

    return Result::Success;
}

void
ServerApp::stop ()
{
    should_stop.set (1);

    mutex.lock ();

    {
	ThreadDataList::iter iter (thread_data_list);
	while (!thread_data_list.iter_done (iter)) {
	    ThreadData * const thread_data = thread_data_list.iter_next (iter)->data;
	    if (!thread_data->poll_group.trigger ())
		logE_ (_func, "poll_group.trigger() failed: ", exc->toString());
	}
    }

    mutex.unlock ();
}

ServerApp::ServerApp (Object * const coderef_container,
		      Count    const num_threads)
    : DependentCodeReferenced (coderef_container),
      timers (firstTimerAdded, this, coderef_container),
      poll_group (coderef_container),
#ifdef LIBMARY_MT_SAFE
      app_poll_group (this),
#endif
      thread_selector (NULL)
{
#ifdef LIBMARY_MT_SAFE
    multi_thread = grab (new MultiThread (
	    num_threads,
	    CbDesc<Thread::ThreadFunc> (threadFunc,
					this /* cb_data */,
					getCoderefContainer(),
					NULL /* ref_data */)));
#endif
}

}

