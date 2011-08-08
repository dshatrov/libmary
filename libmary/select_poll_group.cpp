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


#include <libmary/types.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>

#include <libmary/log.h>
#include <libmary/posix.h>
#include <libmary/util_str.h>
#include <libmary/util_time.h>
#include <libmary/util_posix.h>

#include <libmary/select_poll_group.h>


namespace M {

namespace {
LogGroup libMary_logGroup_select  ("select",         LogLevel::N);
LogGroup libMary_logGroup_time    ("select_time",    LogLevel::N);
LogGroup libMary_logGroup_connerr ("select_connerr", LogLevel::D);
LogGroup libMary_logGroup_iters   ("select_iters",   LogLevel::N);
}

mt_throws Result
SelectPollGroup::triggerPipeWrite ()
{
    logD (select, _func_);

    for (;;) {
	ssize_t const res = write (trigger_pipe [1], "A", 1);
	if (res == -1) {
	    if (errno == EINTR)
		continue;

	    if (errno == EAGAIN || errno == EWOULDBLOCK)
		break;

	    exc_throw <PosixException> (errno);
	    exc_push <InternalException> (InternalException::BackendError);
	    logE_ (_func, "write() failed: ", errnoString (errno));
	    return Result::Failure;
	} else
	if (res != 1 && res != 0) {
	    exc_throw <InternalException> (InternalException::BackendMalfunction);
	    logE_ (_func, "write(): unexpected return value: ", res);
	    return Result::Failure;
	}

	// If res is 0, then we don't care, because this means that the pipe is
	// full of unread data, and the poll group will be triggered by that
	// data	anyway.

	break;
    }

    return Result::Success;
}

void
SelectPollGroup::requestInput (void * const _pollable_entry)
{
    logD (select, _func_);

    PollableEntry * const pollable_entry = static_cast <PollableEntry*> (_pollable_entry);
    // We assume that the poll group is always available when requestInput()
    // is called.
    SelectPollGroup * const self = pollable_entry->select_poll_group;

    assert (self->poll_tlocal);
    if (self->poll_tlocal && self->poll_tlocal == libMary_getThreadLocal()) {
	pollable_entry->need_input = true;
    } else {
	self->mutex.lock ();
	pollable_entry->need_input = true;
	if (self->triggered) {
	    self->mutex.unlock ();
	} else {
	    self->triggered = true;
	    self->mutex.unlock ();
	    self->triggerPipeWrite ();
	}
    }
}

void
SelectPollGroup::requestOutput (void * const _pollable_entry)
{
    logD (select, _func_);

    PollableEntry * const pollable_entry = static_cast <PollableEntry*> (_pollable_entry);
    // We assume that the poll group is always available when requestOutput()
    // is called.
    SelectPollGroup * const self = pollable_entry->select_poll_group;

    assert (self->poll_tlocal);
    if (self->poll_tlocal && self->poll_tlocal == libMary_getThreadLocal()) {
	pollable_entry->need_output = true;
    } else {
	self->mutex.lock ();
	pollable_entry->need_input = true;
	if (self->triggered) {
	    self->mutex.unlock ();
	} else {
	    self->triggered = true;
	    self->mutex.unlock ();
	    self->triggerPipeWrite ();
	}
    }
}

PollGroup::Feedback const SelectPollGroup::pollable_feedback = {
    requestInput,
    requestOutput
};

// The pollable should be available for unsafe callbacks while this method is called.
mt_throws PollGroup::PollableKey
SelectPollGroup::addPollable (CbDesc<Pollable> const &pollable,
			      DeferredProcessor::Registration * const ret_reg)
{
    PollableEntry * const pollable_entry = new PollableEntry;

    logD (select, _func, "cb_data: 0x", fmt_hex, (UintPtr) pollable.cb_data, ", "
	  "pollable_entry: 0x", fmt_hex, (UintPtr) pollable_entry);

    pollable_entry->select_poll_group = this;
    pollable_entry->valid = true;
    pollable_entry->pollable = pollable;
    // We're making an unsafe call, assuming that the pollable is available.
    pollable_entry->fd = pollable->getFd (pollable.cb_data);
    pollable_entry->need_input = true;
    pollable_entry->need_output = true;

    mutex.lock ();
    pollable_list.append (pollable_entry);
    mutex.unlock ();

    // We're making an unsafe call, assuming that the pollable is available.
    //
    // We're counting on the fact that the poll group will always be available
    // when pollable_feedback callbacks are called - that's why we use NULL
    // for coderef_container.
// Deprecated    pollable->setFeedback (Cb<Feedback> (&pollable_feedback, pollable_entry, getCoderefContainer()), pollable.getCbData());
    pollable->setFeedback (
	    Cb<Feedback> (&pollable_feedback, pollable_entry, NULL /* coderef_container */),
	    pollable.cb_data);

    if (ret_reg)
	ret_reg->setDeferredProcessor (&deferred_processor);

    // TODO FIXME if (different thread) then trigger().

    return static_cast <void*> (pollable_entry);
}

void
SelectPollGroup::removePollable (PollableKey const mt_nonnull key)
{
    PollableEntry * const pollable_entry = static_cast <PollableEntry*> (key);

    logD (select, _func, "pollable_entry: 0x", fmt_hex, (UintPtr) pollable_entry);

    mutex.lock ();
    pollable_entry->valid = false;
    pollable_list.remove (pollable_entry);
    pollable_entry->unref ();
    mutex.unlock ();
}

mt_throws Result
SelectPollGroup::poll (Uint64 const timeout_microsec)
    mt_throw (InternalException)
{
    Time const start_microsec = getTimeMicroseconds ();

    logD (time, _func, fmt_hex, "start_microsec: 0x", start_microsec);

    Result ret_res = Result::Success;

    if (!poll_tlocal)
	poll_tlocal = libMary_getThreadLocal();
    else
	assert (poll_tlocal == libMary_getThreadLocal());

    SelectedList selected_list;
    fd_set rfds, wfds, efds;
    for (;;) {
	logD (select, _func, "iteration");

	FD_ZERO (&rfds);
	FD_ZERO (&wfds);
	FD_ZERO (&efds);

	int largest_fd = trigger_pipe [0];
	FD_SET (trigger_pipe [0], &rfds);
	// We hope for the better and don't subscribe for errors on trigger_pipe[*].

	// FIXME There's a limit of 1024 (correct?) fds for a single call to select().

	selected_list.clear ();

	{
	  StateMutexLock l (&mutex);

	    PollableList::iter iter (pollable_list);
	    while (!pollable_list.iter_done (iter)) {
		PollableEntry * const pollable_entry = pollable_list.iter_next (iter);

		if (pollable_entry->fd > largest_fd)
		    largest_fd = pollable_entry->fd;

		FD_SET (pollable_entry->fd, &efds);
		if (pollable_entry->need_input) {
		    logD (select, _func, "adding pollable_entry 0x", fmt_hex, (UintPtr) pollable_entry, " to rfds");
		    FD_SET (pollable_entry->fd, &rfds);
		}
		if (pollable_entry->need_output) {
		    logD (select, _func, "adding pollable_entry 0x", fmt_hex, (UintPtr) pollable_entry, " to wfds");
		    FD_SET (pollable_entry->fd, &wfds);
		}

		selected_list.append (pollable_entry);
		pollable_entry->ref ();
	    }
	}

	Time elapsed_microsec;
	int nfds = 0;
	{
	    Time const cur_microsec = getTimeMicroseconds ();

	    // TEST
	    if (cur_microsec < start_microsec) {
		logE_ (_func, fmt_hex, "cur_microsec: ", cur_microsec, ", start_microsec: ", start_microsec);
		unreachable ();
	    }
	    assert (cur_microsec >= start_microsec);

	    elapsed_microsec = cur_microsec - start_microsec;

	    logD (select, _func, "timeout_microsec: ", timeout_microsec == (Uint64) -1 ? toString ("-1") : toString (timeout_microsec), ", "
		  "elapsed_microsec: ", elapsed_microsec);

	    bool null_timeout = true;
	    struct timeval timeout_val;
	    if (!got_deferred_tasks) {
		if (timeout_microsec != (Uint64) -1) {
		    null_timeout = false;
		    if (timeout_microsec > elapsed_microsec) {
			timeout_val.tv_sec = (timeout_microsec - elapsed_microsec) / 1000000;
			timeout_val.tv_usec = (timeout_microsec - elapsed_microsec) % 1000000;

			logD (select, _func, "tv_sec: ", timeout_val.tv_sec, ", tv_usec: ", timeout_val.tv_usec);
		    } else {
			timeout_val.tv_sec = 0;
			timeout_val.tv_usec = 0;
		    }
		}
	    } else {
		logD (select, "got deferred tasks");
		null_timeout = false;
		timeout_val.tv_sec = 0;
		timeout_val.tv_usec = 0;
	    }

	    nfds = select (largest_fd + 1, &rfds, &wfds, &efds, null_timeout ? NULL : &timeout_val);
	    if (nfds == -1) {
		if (errno == EINTR) {
		    SelectedList::iter iter (selected_list);
		    mutex.lock ();
		    while (!selected_list.iter_done (iter)) {
			PollableEntry * const pollable_entry = selected_list.iter_next (iter);
			pollable_entry->unref ();
		    }
		    mutex.unlock ();

		    // Re-initializing rfds, wfds, efds.
		    continue;
		}

		exc_throw <PosixException> (errno);
		exc_push <InternalException> (InternalException::BackendError);
		logE_ (_func, "select() failed: ", errnoString (errno));
		ret_res = Result::Failure;
		goto _select_interrupted;
	    }

#if 0
// Deprecated.
	    if (nfds == 0) {
	      // Timeout expired.
		got_deferred_tasks = deferred_processor.process ();
		goto _select_interrupted;
	    }
#endif

	    got_deferred_tasks = false;
	}

	if (frontend)
	    frontend.call (frontend->pollIterationBegin);

	if (nfds > 0) {
	    SelectedList::iter iter (selected_list);
	    while (!selected_list.iter_done (iter)) {
		PollableEntry * const pollable_entry = selected_list.iter_next (iter);

		mutex.lock ();

		if (pollable_entry->valid) {
		    Uint32 event_flags = 0;

		    if (FD_ISSET (pollable_entry->fd, &rfds)) {
			pollable_entry->need_input = false;
			event_flags |= Input;
			logD (select, _func, "Input");
		    }

		    if (FD_ISSET (pollable_entry->fd, &wfds)) {
			pollable_entry->need_output = false;
			event_flags |= Output;
			logD (select, _func, "Output");
		    }

		    if (FD_ISSET (pollable_entry->fd, &efds)) {
			event_flags |= Error;
			logE (connerr, _func, "Error, weak object: 0x", fmt_hex, (UintPtr) pollable_entry->pollable.getWeakObject());
		    }

		    if (event_flags) {
			logD (iters, _func, "notifying pollable_entry 0x", fmt_hex, (UintPtr) pollable_entry, ", "
			      "pollable 0x", fmt_hex, (UintPtr) pollable_entry->pollable.getWeakObject());
			mutex.unlock ();
			pollable_entry->pollable.call (pollable_entry->pollable->processEvents, /*(*/ event_flags /*)*/);
			mutex.lock ();
			logD (iters, _func, "notified pollable_entry 0x", fmt_hex, (UintPtr) pollable_entry, ", "
			      "pollable 0x", fmt_hex, (UintPtr) pollable_entry->pollable.getWeakObject());
		    }
		}

		pollable_entry->unref ();

		mutex.unlock ();
	    }
	} // if (nfds > 0)

	if (frontend) {
	    bool extra_iteration_needed = false;
	    frontend.call_ret (&extra_iteration_needed, frontend->pollIterationEnd);
	    if (extra_iteration_needed)
		got_deferred_tasks = true;
	}

	if (deferred_processor.process ())
	    got_deferred_tasks = true;

	if (FD_ISSET (trigger_pipe [0], &rfds)) {
	    for (;;) {
		Byte buf [128];
		ssize_t const res = read (trigger_pipe [0], buf, sizeof (buf));
		if (res == -1) {
		    if (errno == EINTR)
			continue;

		    if (errno == EAGAIN || errno == EWOULDBLOCK)
			break;

		    exc_throw <PosixException> (errno);
		    exc_push <InternalException> (InternalException::BackendError);
		    logE_ (_func, "read() failed (trigger pipe): ", errnoString (errno));
		    ret_res = Result::Failure;
		    goto _select_interrupted;
		} else
		if (res < 0 || (Size) res > sizeof (buf)) {
		    exc_throw <InternalException> (InternalException::BackendMalfunction);
		    logE_ (_func, "read(): unexpected return value (trigger pipe): ", res);
		    ret_res = Result::Failure;
		    goto _select_interrupted;
		}

		if ((Size) res < sizeof (buf)) {
		  // Optimizing away an extra read() syscall.
		    break;
		}
	    }

	    mutex.lock ();
	    triggered = false;
	    mutex.unlock ();

	    break;
	}

	if (elapsed_microsec >= timeout_microsec) {
	  // Timeout expired.
	    break;
	}
    } // for (;;)

    return ret_res;

_select_interrupted:
    SelectedList::iter iter (selected_list);
    mutex.lock ();
    while (!selected_list.iter_done (iter)) {
	PollableEntry * const pollable_entry = selected_list.iter_next (iter);
	pollable_entry->unref ();
    }
    mutex.unlock ();

    return ret_res;
}

mt_throws Result
SelectPollGroup::trigger ()
{
    logD (select, _func_);

    mutex.lock ();
    if (triggered) {
	mutex.unlock ();
	return Result::Success;
    }
    triggered = true;
    mutex.unlock ();

    return triggerPipeWrite ();
}

mt_throws Result
SelectPollGroup::open ()
{
    logD (select, _func_);

    if (!posix_createNonblockingPipe (&trigger_pipe))
	return Result::Failure;

    return Result::Success;
}

SelectPollGroup::SelectPollGroup (Object * const coderef_container)
    : DependentCodeReferenced (coderef_container),
      triggered (false),
      // Initializing to 'true' to process deferred tasks scheduled before we
      // enter poll() the first time.
      got_deferred_tasks (true),
      poll_tlocal (NULL)
{
    trigger_pipe [0] = -1;
    trigger_pipe [1] = -1;
}

SelectPollGroup::~SelectPollGroup ()
{
    logD (select, _func_);

    mutex.lock ();
    {
	PollableList::iter iter (pollable_list);
	while (!pollable_list.iter_done (iter)) {
	    PollableEntry * const pollable_entry = pollable_list.iter_next (iter);
	    delete pollable_entry;
	}
    }
    mutex.unlock ();

    for (int i = 0; i < 2; ++i) {
	if (trigger_pipe [i] != -1) {
	    for (;;) {
		int const res = close (trigger_pipe [i]);
		if (res == -1) {
		    if (errno == EINTR)
			continue;

		    logE_ (_func, "trigger_pipe[", i, "]: close() failed: ", errnoString (errno));
		} else
		if (res != 0) {
		    logE_ (_func, "trigger_pipe[", i, "]: close(): unexpected return value: ", res);
		}

		break;
	    }
	}
    }
}

}

