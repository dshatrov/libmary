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
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>

#include <libmary/util_posix.h>
#include <libmary/log.h>

#include <libmary/epoll_poll_group.h>


namespace M {

void
EpollPollGroup::processPollableDeletionQueue ()
{
    PollableDeletionQueue::iter iter (pollable_deletion_queue);
    while (!pollable_deletion_queue.iter_done (iter)) {
	PollableEntry * const pollable_entry = pollable_deletion_queue.iter_next (iter);
	delete pollable_entry;
    }
    pollable_deletion_queue.clear ();
}

mt_throws Result
EpollPollGroup::triggerPipeWrite ()
{
    return commonTriggerPipeWrite (trigger_pipe [1]);
}

mt_throws PollGroup::PollableKey
EpollPollGroup::addPollable (CbDesc<Pollable> const &pollable,
			     DeferredProcessor::Registration * const ret_reg)
{
    PollableEntry * const pollable_entry = new PollableEntry;
    pollable_entry->epoll_poll_group = this;
    pollable_entry->pollable = pollable;
    // We're making an unsafe call, assuming that the pollable is available.
    pollable_entry->fd = pollable->getFd (pollable.cb_data);
    pollable_entry->valid = true;

    mutex.lock ();
    pollable_list.append (pollable_entry);
    mutex.unlock ();

    {
	struct epoll_event event;
	event.events = EPOLLET | EPOLLIN | EPOLLOUT | EPOLLRDHUP;
	event.data.ptr = pollable_entry;
	int const res = epoll_ctl (efd, EPOLL_CTL_ADD, pollable_entry->fd, &event);
	if (res == -1) {
	    exc_throw <PosixException> (errno);
	    logE_ (_func, "epoll_ctl() failed: ", errnoString (errno));
	    goto _failure;
	}

	if (res != 0) {
	    exc_throw <InternalException> (InternalException::BackendMalfunction);
	    logE_ (_func, "epoll_ctl(): unexpected return value: ", res);
	    goto _failure;
	}
    }

    // TODO FIXME if (different thread) then trigger(). Check if this is really necessary with epoll.

    if (ret_reg)
	ret_reg->setDeferredProcessor (&deferred_processor);

    return pollable_entry;

_failure:
    mutex.lock ();
    pollable_list.remove (pollable_entry);
    mutex.unlock ();
    delete pollable_entry;

    return NULL;
}

void
EpollPollGroup::removePollable (PollableKey const mt_nonnull key)
{
    PollableEntry * const pollable_entry = static_cast <PollableEntry*> (key);

    {
	int const res = epoll_ctl (efd, EPOLL_CTL_DEL, pollable_entry->fd, NULL /* event */);
	if (res == -1) {
	    logE_ (_func, "epoll_ctl() failed: ", errnoString (errno));
	} else {
	    if (res != 0)
		logE_ (_func, "epoll_ctl(): unexpected return value: ", res);
	}
    }

    mutex.lock ();
    pollable_entry->valid = false;
    pollable_list.remove (pollable_entry);
    pollable_deletion_queue.append (pollable_entry);
    mutex.unlock ();
}

mt_throws Result
EpollPollGroup::poll (Uint64 const timeout_microsec)
{
//    logD_ (_func, "timeout: ", timeout_microsec);

    Time const start_microsec = getTimeMicroseconds ();

    struct epoll_event events [4096];
    for (;;) {
	Time const cur_microsec = getTimeMicroseconds ();

	Time const elapsed_microsec = cur_microsec - start_microsec;

//	logD_ (_func, "start: ", start_microsec, ", cur: ", cur_microsec, ", elapsed: ", elapsed_microsec);

	int timeout;
	if (!got_deferred_tasks) {
	    if (timeout_microsec != (Uint64) -1) {
		if (timeout_microsec > elapsed_microsec) {
		    timeout = (timeout_microsec - elapsed_microsec) / 1000;
		    if (timeout == 0)
			timeout = 1;
		} else {
		    timeout = 0;
		}
	    } else {
		timeout = -1;
	    }
	} else {
	    // We've got deferred tasks to process, hence we shouldn't block.
	    timeout = 0;
	}

	int const nfds = epoll_wait (efd, events, sizeof (events) / sizeof (events [0]), timeout);
	if (nfds == -1) {
	    if (errno == EINTR)
		continue;

	    exc_throw <PosixException> (errno);
	    logE_ (_func, "epoll_wait() failed: ", errnoString (errno));
	    return Result::Failure;
	}

	if (nfds < 0 || (Size) nfds > sizeof (events) / sizeof (events [0])) {
	    logE_ (_func, "epoll_wait(): unexpected return value: ", nfds);
	    return Result::Failure;
	}

#if 0
// Deprecated.
	if (nfds == 0) {
	  // Timeout expired.
	    got_deferred_tasks = deferred_processor.process ();
	    break;
	}
#endif

	got_deferred_tasks = false;

	if (frontend)
	    frontend.call (frontend->pollIterationBegin);

	bool trigger_pipe_ready = false;

	if (nfds > 0) {
	    mutex.lock ();
	    bool const saved_triggered = triggered;
	    triggered = true;
	    for (int i = 0; i < nfds; ++i) {
		PollableEntry * const pollable_entry = static_cast <PollableEntry*> (events [i].data.ptr);
		uint32_t const epoll_event_flags = events [i].events;
		Uint32 event_flags = 0;

		if (pollable_entry == NULL) {
		  // Trigger pipe event.
		    if (epoll_event_flags & EPOLLIN)
			trigger_pipe_ready = true;

		    if (epoll_event_flags & EPOLLOUT)
			logW_ (_func, "Unexpected EPOLLOUT event for trigger pipe");

		    if (epoll_event_flags & EPOLLHUP   ||
			epoll_event_flags & EPOLLRDHUP ||
			epoll_event_flags & EPOLLERR)
		    {
			logE_ (_func, "Trigger pipe error: 0x", fmt_hex, epoll_event_flags);
		    }

		    continue;
		}

		if (pollable_entry->valid) {
		    if (epoll_event_flags & EPOLLIN)
			event_flags |= PollGroup::Input;

		    if (epoll_event_flags & EPOLLOUT)
			event_flags |= PollGroup::Output;

		    if (epoll_event_flags & EPOLLHUP ||
			epoll_event_flags & EPOLLRDHUP)
		    {
			event_flags |= PollGroup::Hup;
		    }

		    if (epoll_event_flags & EPOLLERR)
			event_flags |= PollGroup::Error;

		    if (event_flags) {
			mutex.unlock ();
			pollable_entry->pollable.call (pollable_entry->pollable->processEvents, /* ( */ event_flags /* ) */);
			mutex.lock ();
		    }
		}
	    } // for (;;) - for all fds.

	    processPollableDeletionQueue ();

	    triggered = saved_triggered;
	    mutex.unlock ();
	} // if (nfds > 0)

	if (frontend) {
	    bool extra_iteration_needed = false;
	    frontend.call_ret (&extra_iteration_needed, frontend->pollIterationEnd);
	    if (extra_iteration_needed)
		got_deferred_tasks = true;
	}

	if (deferred_processor.process ())
	    got_deferred_tasks = true;

	if (trigger_pipe_ready) {
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
		    return Result::Failure;
		} else
		if (res < 0 || (Size) res > sizeof (buf)) {
		    exc_throw <InternalException> (InternalException::BackendMalfunction);
		    logE_ (_func, "read(): unexpected return value (trigger pipe): ", res);
		    return Result::Failure;
		}

		if ((Size) res < sizeof (buf)) {
		  // Optimizing away an extra read() syscall.
		    break;
		}
	    } // for (;;)

	    mutex.lock ();
	    triggered = false;
	    mutex.unlock ();

	    break;
	}

	if (elapsed_microsec >= timeout_microsec) {
	  // Timeout expired.
	    break;
	}
    }

    return Result::Success;
}

mt_throws Result
EpollPollGroup::trigger ()
{
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
EpollPollGroup::open ()
{
    efd =  epoll_create (1 /* size, unused */);
    if (efd == -1) {
	exc_throw <PosixException> (errno);
	logE_ (_func, "epoll_create() failed: ", errnoString (errno));
	return Result::Failure;
    }

    if (!posix_createNonblockingPipe (&trigger_pipe))
	return Result::Failure;

    {
	struct epoll_event event;
	event.events = EPOLLET | EPOLLIN | EPOLLRDHUP;
	event.data.ptr = NULL; // 'NULL' tells that this is trigger pipe.
	int const res = epoll_ctl (efd, EPOLL_CTL_ADD, trigger_pipe [0], &event);
	if (res == -1) {
	    exc_throw <PosixException> (errno);
	    logE_ (_func, "epoll_ctl() failed: ", errnoString (errno));
	    return Result::Failure;
	}

	if (res != 0) {
	    exc_throw <InternalException> (InternalException::BackendMalfunction);
	    logE_ (_func, "epoll_ctl(): unexpected return value: ", res);
	    return Result::Failure;
	}
    }

    return Result::Success;
}

EpollPollGroup::EpollPollGroup (Object * const coderef_container)
    : DependentCodeReferenced (coderef_container),
      efd (-1),
      triggered (false),
      // Initializing to 'true' to process deferred tasks scheduled before we
      // enter poll() the first time.
      got_deferred_tasks (true)
{
    trigger_pipe [0] = -1;
    trigger_pipe [1] = -1;
}

EpollPollGroup::~EpollPollGroup ()
{
    mutex.lock ();

    {
	PollableList::iter iter (pollable_list);
	while (!pollable_list.iter_done (iter)) {
	    PollableEntry * const pollable_entry = pollable_list.iter_next (iter);
	    delete pollable_entry;
	}
    }

    {
	PollableDeletionQueue::iter iter (pollable_deletion_queue);
	while (!pollable_deletion_queue.iter_done (iter)) {
	    PollableEntry * const pollable_entry = pollable_deletion_queue.iter_next (iter);
	    delete pollable_entry;
	}
    }

    mutex.unlock ();

    if (efd != -1) {
	for (;;) {
	    int const res = close (efd);
	    if (res == -1) {
		if (errno == EINTR)
		    continue;

		logE_ (_func, "close() failed (efd): ", errnoString (errno));
	    } else
	    if (res != 0) {
		logE_ (_func, "close() (efd): unexpected return value: ", res);
	    }

	    break;
	}
    }

    for (int i = 0; i < 2; ++i) {
	if (trigger_pipe [i] != -1) {
	    for (;;) {
		int const res = close (trigger_pipe [i]);
		if (res == -1) {
		    if (errno == EINTR)
			continue;

		    logE_ (_func, "close() failed (trigger_pipe[", i, "]): ", errnoString (errno));
		} else
		if (res != 0) {
		    logE_ (_func, "close(): unexpected return value (trigger_pipe[", i, "]): ", res);
		}

		break;
	    }
	}
    }
}

}

