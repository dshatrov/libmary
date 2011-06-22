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


#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>

#include <libmary/log.h>
#include <libmary/posix.h>
#include <libmary/util_str.h>
#include <libmary/util_time.h>

#include <libmary/select_poll_group.h>


namespace M {

namespace {
LogGroup libMary_logGroup_time    ("time",    LogLevel::N);
LogGroup libMary_logGroup_connerr ("connerr", LogLevel::D);
LogGroup libMary_logGroup_iters   ("iters",   LogLevel::N);
}

mt_throws Result
SelectPollGroup::triggerWrite ()
{
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

	// If res is 0, the we don't care, because this means that the pipe is
	// full of unread data, and the poll group will be triggered by that
	// data	anyway.

	break;
    }

    return Result::Success;
}

void
SelectPollGroup::requestInput (void * const _pollable_entry)
{
    PollableEntry * const pollable_entry = static_cast <PollableEntry*> (_pollable_entry);
    SelectPollGroup * const self = pollable_entry->select_poll_group;

    if (self->poll_tlocal && self->poll_tlocal == libMary_getThreadLocal()) {
	pollable_entry->need_input = true;
    } else {
	self->mutex.lock ();
	pollable_entry->need_input = true;
	if (self->triggered) {
	    self->mutex.unlock ();
	} else {
	    self->mutex.unlock ();
	    self->triggerWrite ();
	}
    }
}

void
SelectPollGroup::requestOutput (void * const _pollable_entry)
{
    PollableEntry * const pollable_entry = static_cast <PollableEntry*> (_pollable_entry);
    SelectPollGroup * const self = pollable_entry->select_poll_group;

    if (self->poll_tlocal && self->poll_tlocal == libMary_getThreadLocal()) {
	pollable_entry->need_output = true;
    } else {
	self->mutex.lock ();
	pollable_entry->need_input = true;
	if (self->triggered) {
	    self->mutex.unlock ();
	} else {
	    self->mutex.unlock ();
	    self->triggerWrite ();
	}
    }
}

PollGroup::Feedback const SelectPollGroup::pollable_feedback = {
    requestInput,
    requestOutput
};

// The pollable should be available for unsafe callbacks while this method is called.
mt_throws PollGroup::PollableKey
SelectPollGroup::addPollable (Cb<Pollable> const &pollable)
{
    PollableEntry * const pollable_entry = new PollableEntry;
    pollable_entry->select_poll_group = this;
    pollable_entry->valid = true;
    pollable_entry->pollable = pollable;
    // We're making an unsafe call, assuming that the pollable is available.
    pollable_entry->fd = pollable->getFd (pollable.getCbData());
    pollable_entry->need_input = true;
    pollable_entry->need_output = true;

    mutex.lock ();
    pollable_list.append (pollable_entry);
    mutex.unlock ();

    // We're making an unsafe call, assuming that the pollable is available.
    pollable->setFeedback (Cb<Feedback> (&pollable_feedback, pollable_entry, getCoderefContainer()), pollable.getCbData());

    return static_cast <void*> (pollable_entry);
}

mt_throws Result
SelectPollGroup::removePollable (PollableKey const mt_nonnull key)
{
    PollableEntry * const pollable_entry = static_cast <PollableEntry*> (key);

    mutex.lock ();
    pollable_entry->valid = false;
    pollable_list.remove (pollable_entry);
    mutex.unlock ();

    pollable_entry->unref ();
    return Result::Success;
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
	FD_ZERO (&rfds);
	FD_ZERO (&wfds);
	FD_ZERO (&efds);

	int largest_fd = trigger_pipe [0];
	FD_SET (trigger_pipe [0], &rfds);
	// We hope for the better and don't subscribe for errors on trigger_pipe[*].

	selected_list.clear ();

	{
	  StateMutexLock l (&mutex);

	    PollableList::iter iter (pollable_list);
	    while (!pollable_list.iter_done (iter)) {
		PollableEntry * const pollable_entry = pollable_list.iter_next (iter);

		if (pollable_entry->fd > largest_fd)
		    largest_fd = pollable_entry->fd;

		FD_SET (pollable_entry->fd, &efds);
		if (pollable_entry->need_input)
		    FD_SET (pollable_entry->fd, &rfds);
		if (pollable_entry->need_output)
		    FD_SET (pollable_entry->fd, &wfds);

		selected_list.append (pollable_entry);
		pollable_entry->ref ();
	    }
	}

	{
	    Time const cur_microsec = getTimeMicroseconds ();

	    // TEST
	    if (cur_microsec < start_microsec) {
		logE_ (_func, fmt_hex, "cur_microsec: ", cur_microsec, ", start_microsec: ", start_microsec);
		unreachable ();
	    }
	    assert (cur_microsec >= start_microsec);

	    Time const elapsed_microsec = cur_microsec - start_microsec;

//	    logD_ (_func, "timeout_microsec: ", timeout_microsec == (Uint64) -1 ? toString ("-1") : toString (timeout_microsec), ", elapsed_microsec: ", elapsed_microsec);

	    struct timeval timeout_val;
	    if (timeout_microsec != (Uint64) -1) {
		if (timeout_microsec > elapsed_microsec) {
		    timeout_val.tv_sec = (timeout_microsec - elapsed_microsec) / 1000000;
		    timeout_val.tv_usec = (timeout_microsec - elapsed_microsec) % 1000000;

//		    logD_ (_func, "tv_sec: ", timeout_val.tv_sec, ", tv_usec: ", timeout_val.tv_usec);
		} else {
		    timeout_val.tv_sec = 0;
		    timeout_val.tv_usec = 0;
		}
	    }

	    int const nfds = select (largest_fd + 1, &rfds, &wfds, &efds, timeout_microsec != (Uint64) -1 ? &timeout_val : NULL);
	    if (nfds == -1) {
		if (errno == EINTR) {
		    SelectedList::iter iter (selected_list);
		    while (!selected_list.iter_done (iter)) {
			PollableEntry * const pollable_entry = selected_list.iter_next (iter);
			pollable_entry->unref ();
		    }

		    // Re-initializing rfds, wfds, efds.
		    continue;
		}

		exc_throw <PosixException> (errno);
		exc_push <InternalException> (InternalException::BackendError);
		logE_ (_func, "select() failed: ", errnoString (errno));
		ret_res = Result::Failure;
		goto _select_interrupted;
	    }

	    if (nfds == 0) {
	      // Timeout expired.
		goto _select_interrupted;
	    }
	}

	if (frontend)
	    frontend.call (frontend->pollIterationBegin);

	{
	    SelectedList::iter iter (selected_list);
	    while (!selected_list.iter_done (iter)) {
		PollableEntry * const pollable_entry = selected_list.iter_next (iter);

		mutex.lock ();
		if (pollable_entry->valid) {
		    Uint32 event_flags = 0;

		    if (FD_ISSET (pollable_entry->fd, &rfds)) {
			pollable_entry->need_input = false;
			event_flags |= Input;
		    }

		    if (FD_ISSET (pollable_entry->fd, &wfds)) {
			pollable_entry->need_output = false;
			event_flags |= Output;
		    }

		    if (FD_ISSET (pollable_entry->fd, &efds)) {
			event_flags |= Error;
			logE (connerr, _func, "Error, weak object: 0x", fmt_hex, (UintPtr) pollable_entry->pollable.getWeakObject());
		    }

		    if (event_flags) {
			logD (iters, _func, "notifying pollable 0x", fmt_hex, (UintPtr) pollable_entry->pollable.getWeakObject());
			mutex.unlock ();
			pollable_entry->pollable.call (pollable_entry->pollable->processEvents, /*(*/ event_flags /*)*/);
			mutex.lock ();
			logD (iters, _func, "notified pollable 0x", fmt_hex, (UintPtr) pollable_entry->pollable.getWeakObject());
		    }
		}
		mutex.unlock ();

		pollable_entry->unref ();
	    }
	}

	if (frontend)
	    frontend.call (frontend->pollIterationEnd);

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
    } // for (;;)

    return ret_res;

_select_interrupted:
    SelectedList::iter iter (selected_list);
    while (!selected_list.iter_done (iter)) {
	PollableEntry * const pollable_entry = selected_list.iter_next (iter);
	pollable_entry->unref ();
    }

    return ret_res;
}

mt_throws Result
SelectPollGroup::trigger ()
{
    mutex.lock ();
    if (triggered) {
	mutex.unlock ();
	return Result::Success;
    }
    triggered = true;
    mutex.unlock ();

    return triggerWrite ();
}

mt_throws Result
SelectPollGroup::open ()
{
    {
	int const res = pipe (trigger_pipe);
	if (res == -1) {
	    exc_throw <PosixException> (errno);
	    exc_push <InternalException> (InternalException::BackendError);
	    logE_ (_func, "pipe() failed: ", errnoString (errno));
	    return Result::Failure;
	} else
	if (res != 0) {
	    exc_throw <InternalException> (InternalException::BackendMalfunction);
	    logE_ (_func, "pipe(): unexpected return value: ", res);
	    return Result::Failure;
	}
    }

    for (int i = 0; i < 2; ++i) {
	int flags = fcntl (trigger_pipe [i], F_GETFL, 0);
	if (flags == -1) {
	    exc_throw <PosixException> (errno);
	    exc_push <InternalException> (InternalException::BackendError);
	    logE_ (_func, "fcntl() failed (trigger_pipe[", i, "]): ", errnoString (errno));
	    return Result::Failure;
	}

	flags |= O_NONBLOCK;

	if (fcntl (trigger_pipe [i], F_SETFL, flags) == -1) {
	    exc_throw <PosixException> (errno);
	    exc_push <InternalException> (InternalException::BackendError);
	    logE_ (_func, "fcntl() failed (F_SETFL, trigger_pipe[", i, "]): ", errnoString (errno));
	    return Result::Failure;
	}
    }

    return Result::Success;
}

SelectPollGroup::~SelectPollGroup ()
{
    mutex.lock ();

    PollableList::iter iter (pollable_list);
    while (!pollable_list.iter_done (iter)) {
	PollableEntry * const pollable_entry = pollable_list.iter_next (iter);
	delete pollable_entry;
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

