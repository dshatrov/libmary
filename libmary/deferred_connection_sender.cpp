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
#include <sys/uio.h>
#include <errno.h>

#include <libmary/mwritev.h>
#include <libmary/log.h>

#include <libmary/deferred_connection_sender.h>


namespace M {

namespace {
LogGroup libMary_logGroup_mwritev ("deferred_sender_mwritev", LogLevel::N);
}

DeferredConnectionSender::OutputQueue DeferredConnectionSender::glob_output_queue;
bool DeferredConnectionSender::glob_output_queue_processing = false;
Mutex DeferredConnectionSender::glob_output_queue_mutex;

#ifdef LIBMARY_ENABLE_MWRITEV
namespace {

enum {
    Mwritev_MaxFds       = 4096,
    Mwritev_MaxTotalIovs = 65536,
    Mwritev_MaxIovsPerFd = 1024
};

mt_sync (DeferredSender::pollIterationEnd)
mt_begin
    bool           mwritev_initialized = false;
    int           *mwritev_fds;
    struct iovec **mwritev_iovs;
    struct iovec  *mwritev_iovs_heap;
    int           *mwritev_num_iovs;
    int           *mwritev_res;
mt_end

void mwritevInit ()
{
    mwritev_initialized = true;

    mwritev_fds = new int [Mwritev_MaxFds];
    assert (mwritev_fds);

    mwritev_iovs = new struct iovec* [Mwritev_MaxFds];
    assert (mwritev_iovs);

    mwritev_iovs_heap = new struct iovec [Mwritev_MaxTotalIovs];
    assert (mwritev_iovs_heap);

    mwritev_num_iovs = new int [Mwritev_MaxFds];
    assert (mwritev_num_iovs);

    mwritev_res = new int [Mwritev_MaxFds];
    assert (mwritev_res);
}

} // namespace {}
#endif

Connection::OutputFrontend const DeferredConnectionSender::conn_output_frontend = {
    processOutput
};

mt_mutex (mutex) mt_unlocks (mutex) void
DeferredConnectionSender::toGlobOutputQueue (bool const add_ref)
{
    if (in_output_queue) {
	mutex.unlock ();
	return;
    }

    in_output_queue = true;
    mutex.unlock ();

    glob_output_queue_mutex.lock ();
    glob_output_queue.append (this);
    glob_output_queue_mutex.unlock ();

    if (add_ref) {
	Object * const coderef_container = getCoderefContainer();
	if (coderef_container)
	    coderef_container->ref ();
    }
}

mt_mutex (mutex) mt_unlocks (mutex) void
DeferredConnectionSender::closeIfNeeded ()
{
    if (close_after_flush &&
	!conn_sender_impl.gotDataToSend ())
    {
	mutex.unlock ();

	if (frontend && frontend->closed)
	    frontend.call (frontend->closed, /*(*/ (Exception*) NULL /* exc_ */);
    } else {
	mutex.unlock ();
    }
}

void
DeferredConnectionSender::processOutput (void * const _self)
{
    DeferredConnectionSender * const self = static_cast <DeferredConnectionSender*> (_self);

    self->mutex.lock ();
    self->ready_for_output = true;
    if (self->conn_sender_impl.gotDataToSend ())
	mt_unlocks (self->mutex) self->toGlobOutputQueue (true /* add_ref */);
    else
	self->mutex.unlock ();
}

void
DeferredConnectionSender::sendMessage (MessageEntry * const mt_nonnull msg_entry)
{
    logD (mwritev, _func, "msg_entry: 0x", fmt_hex, (UintPtr) msg_entry);

    mutex.lock ();
    conn_sender_impl.queueMessage (msg_entry);
    mutex.unlock ();
}

void
DeferredConnectionSender::flush ()
{
    mutex.lock ();
    if (ready_for_output
	&& conn_sender_impl.gotDataToSend ())
    {
	mt_unlocks (mutex) toGlobOutputQueue (true /* add_ref */);
    } else {
	mutex.unlock ();
    }
}

void
DeferredConnectionSender::closeAfterFlush ()
{
    mutex.lock ();
    close_after_flush = true;
    mt_unlocks (mutex) closeIfNeeded ();
}

bool
DeferredConnectionSender::pollIterationEnd ()
{
  // Processing global output queue.

    ProcessingQueue processing_queue;

    glob_output_queue_mutex.lock ();

    if (glob_output_queue_processing) {
	glob_output_queue_mutex.unlock ();
	logW_ (_func, "Concurrent invocation");
	return false;
    }
    glob_output_queue_processing = true;

    {
	OutputQueue::iter iter (glob_output_queue);
	while (!glob_output_queue.iter_done (iter)) {
	    DeferredConnectionSender * const deferred_sender = glob_output_queue.iter_next (iter);
	    // Note that deferred_sender->mutex is not locked here.

	    processing_queue.append (deferred_sender);
	    glob_output_queue.remove (deferred_sender);
	}
    }

    glob_output_queue_mutex.unlock ();

    bool extra_iteration_needed = false;

    ProcessingQueue::iter iter (processing_queue);
    while (!processing_queue.iter_done (iter)) {
	DeferredConnectionSender * const deferred_sender = processing_queue.iter_next (iter);

	// The only place where 'deferred_sender' may be removed from the queue
	// is its destructor, which won't be called because 'deferred_sender' is
	// refed here.
	deferred_sender->mutex.lock ();

	assert (deferred_sender->in_output_queue);
	deferred_sender->in_output_queue = false;

	deferred_sender->conn_sender_impl.markProcessingBarrier ();

	AsyncIoResult const res = deferred_sender->conn_sender_impl.sendPendingMessages ();
	if (res == AsyncIoResult::Error ||
	    res == AsyncIoResult::Eof)
	{
	    deferred_sender->ready_for_output = false;

	    // exc is NULL for Eof.
	    if (res == AsyncIoResult::Error)
		logE_ (_func, exc->toString());

	    if (deferred_sender->frontend && deferred_sender->frontend->closed) {
		deferred_sender->mutex.unlock ();
		deferred_sender->frontend.call (deferred_sender->frontend->closed, /*(*/ exc /*)*/);
	    } else {
		deferred_sender->mutex.unlock ();
	    }

	    {
		Object * const coderef_container = deferred_sender->getCoderefContainer();
		if (coderef_container)
		    coderef_container->unref ();
	    }
	    continue;
	}

	if (res == AsyncIoResult::Again)
	    deferred_sender->ready_for_output = false;
	else
	    deferred_sender->ready_for_output = true;

	bool const tmp_extra_iteration_needed = deferred_sender->conn_sender_impl.processingBarrierHit();
//	logD_ (_func, "tmp_extra_iteration_needed: ", tmp_extra_iteration_needed ? "true" : "false");

	if (!tmp_extra_iteration_needed) {
	  // At this point, conn_sender_impl has either sent all data, or it has
	  // gotten EAGAIN from writev. In either case, we should have removed
	  // deferred_sender from glob_output_queue.

	    mt_unlocks (deferred_sender->mutex) deferred_sender->closeIfNeeded ();

	    Object * const coderef_container = deferred_sender->getCoderefContainer();
	    if (coderef_container)
		coderef_container->unref ();
	} else {
	    mt_unlocks (deferred_sender->mutex) deferred_sender->toGlobOutputQueue (false /* add_ref */);
	    extra_iteration_needed = true;
	}
    }

    glob_output_queue_mutex.lock ();
    if (!glob_output_queue.isEmpty())
	extra_iteration_needed = true;

    glob_output_queue_processing = false;
    glob_output_queue_mutex.unlock ();

//    logD_ (_func, "extra_iteration_needed: ", extra_iteration_needed ? "true" : "false");
    return extra_iteration_needed;
}

#ifdef LIBMARY_ENABLE_MWRITEV
bool
DeferredConnectionSender::pollIterationEnd_mwritev ()
{
    ProcessingQueue processing_queue;

    glob_output_queue_mutex.lock ();

    if (!mwritev_initialized)
	mwritevInit ();

    if (glob_output_queue_processing) {
	glob_output_queue_mutex.unlock ();
	logW_ (_func, "Concurrent invocation");
	return false;
    }
    glob_output_queue_processing = true;

    {
	OutputQueue::iter iter (glob_output_queue);
	while (!glob_output_queue.iter_done (iter)) {
	    DeferredConnectionSender * const deferred_sender = glob_output_queue.iter_next (iter);
	    processing_queue.append (deferred_sender);
	    glob_output_queue.remove (deferred_sender);
	}
    }

    glob_output_queue_mutex.unlock ();

    bool extra_iteration_needed = false;

    ProcessingQueue::iter start_iter (processing_queue);
    while (!processing_queue.iter_done (start_iter)) {
	ProcessingQueue::iter next_iter;

	Count fd_idx = 0;
	{
	    assert (Mwritev_MaxTotalIovs >= Mwritev_MaxIovsPerFd);
	    Count total_iovs = 0;
	    ProcessingQueue::iter iter = start_iter;
	    while (!processing_queue.iter_done (iter)) {
		DeferredConnectionSender * const deferred_sender = processing_queue.iter_next (iter);

		mwritev_fds [fd_idx] = deferred_sender->conn_sender_impl.getConnection()->getFd();

		deferred_sender->mutex.lock ();

		assert (deferred_sender->in_output_queue);
		deferred_sender->in_output_queue = false;

		deferred_sender->conn_sender_impl.markProcessingBarrier ();

		Count num_iovs = 0;
		deferred_sender->conn_sender_impl.sendPendingMessages_fillIovs (&num_iovs,
										mwritev_iovs_heap + total_iovs,
										Mwritev_MaxIovsPerFd);

		// TODO Try leaving the lock here and unlocking after mwritev() call.
		// TEST (uncomment)
//		deferred_sender->mutex.unlock ();

		mwritev_iovs [fd_idx] = mwritev_iovs_heap + total_iovs;
		mwritev_num_iovs [fd_idx] = num_iovs;

		total_iovs += num_iovs;

		++fd_idx;

		if (Mwritev_MaxTotalIovs - total_iovs < Mwritev_MaxIovsPerFd) {
		    logD (mwritev, _func, "max total iovs");
		    break;
		}
	    }

	    next_iter = iter;
	}

	{
	    Result const res = libMary_mwritev (fd_idx,
						mwritev_fds,
						mwritev_iovs,
						mwritev_num_iovs,
						mwritev_res);
	    if (!res) {
		logE_ (_func, "libMary_mwritev() failed: ", errnoString (errno));
		glob_output_queue_processing = false;
		glob_output_queue_mutex.unlock ();
		return false;
	    }
	}

	fd_idx = 0;
	{
	    ProcessingQueue::iter iter = start_iter;
	    while (!processing_queue.iter_done (iter)) {
		DeferredConnectionSender * const deferred_sender = processing_queue.iter_next (iter);

		bool eintr = false;
		Size num_written = 0;
		int const posix_res = mwritev_res [fd_idx];
		AsyncIoResult async_res;
		if (posix_res >= 0) {
		    num_written = (Size) posix_res;
		    async_res = AsyncIoResult::Normal;
		} else
		if (posix_res == -EAGAIN ||
		    posix_res == -EWOULDBLOCK)
		{
		    async_res = AsyncIoResult::Again;
		} else
		if (posix_res == -EINTR) {
		    eintr = true;
		} else
		if (posix_res == -EPIPE) {
		    async_res = AsyncIoResult::Eof;
		} else  {
		    async_res = AsyncIoResult::Error;
		}

		logD (mwritev, _func, "async_res: ", async_res);

		if (async_res == AsyncIoResult::Error ||
		    async_res == AsyncIoResult::Eof)
		{
		    // TODO Questionable lock.
		    // TEST (uncomment)
//		    deferred_sender->mutex.lock ();
		    deferred_sender->ready_for_output = false;

		    // exc is NULL for Eof.
		    if (async_res == AsyncIoResult::Error)
			logE_ (_func, exc->toString());

		    if (deferred_sender->frontend && deferred_sender->frontend->closed) {
			deferred_sender->mutex.unlock ();
			deferred_sender->frontend.call (deferred_sender->frontend->closed, /*(*/ exc /*)*/);
		    } else {
			deferred_sender->mutex.unlock ();
		    }

		    {
			Object * const coderef_container = deferred_sender->getCoderefContainer();
			if (coderef_container)
			    coderef_container->unref ();
		    }
		    continue;
		}

		if (async_res == AsyncIoResult::Again)
		    deferred_sender->ready_for_output = false;
		else
		    deferred_sender->ready_for_output = true;

		bool tmp_extra_iteration_needed;
		if (!eintr) {
		    // TEST (uncomment)
//		    deferred_sender->mutex.lock ();
		    deferred_sender->conn_sender_impl.sendPendingMessages_react (async_res, num_written);
		    tmp_extra_iteration_needed = deferred_sender->conn_sender_impl.processingBarrierHit();
		} else {
		    tmp_extra_iteration_needed = true;
		}

		if (!tmp_extra_iteration_needed) {
		    mt_unlocks (deferred_sender->mutex) deferred_sender->closeIfNeeded ();

		    Object * const coderef_container = deferred_sender->getCoderefContainer ();
		    if (coderef_container)
			coderef_container->unref ();
		} else {
		    mt_unlocks (deferred_sender->mtuex) deferred_sender->closeIfNeeded ();
		    extra_iteration_needed = true;
		}

		++fd_idx;
	    }
	}

	start_iter = next_iter;
    }

    glob_output_queue_mutex.lock ();
    if (!glob_output_queue.isEmpty())
	extra_iteration_needed = true;

    glob_output_queue_processing = false;
    glob_output_queue_mutex.unlock ();

    return extra_iteration_needed;
}
#endif

DeferredConnectionSender::~DeferredConnectionSender ()
{
    // Doing lock/unlock to ensure that ~ConnectionSenderImpl() will see correct
    // data.
    mutex.lock();

    // Currently, DeferredConnectionSender cannot be destroyed while it is
    // in glob_output_queue, because it is referenced while it is in the queue.
    assert (!in_output_queue);

    mutex.unlock();
}

}

