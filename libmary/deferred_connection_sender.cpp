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

#include <libmary/types.h>
#include <sys/uio.h>
#include <errno.h>

#ifdef LIBMARY_ENABLE_MWRITEV
#include <libmary/mwritev.h>
#endif
#include <libmary/log.h>

#include <libmary/deferred_connection_sender.h>


namespace M {

namespace {
LogGroup libMary_logGroup_mwritev ("deferred_sender_mwritev", LogLevel::I);
LogGroup libMary_logGroup_sender ("deferred_sender", LogLevel::I);
LogGroup libMary_logGroup_close ("deferred_sender_close", LogLevel::I);
}

#ifdef LIBMARY_ENABLE_MWRITEV
namespace {

enum {
    Mwritev_MaxFds       = 4096,
    Mwritev_MaxTotalIovs = 65536,
    Mwritev_MaxIovsPerFd = 1024
};

mt_sync (DeferredSender::pollIterationEnd)
mt_begin
mt_end

void mwritevInit (LibMary_MwritevData * const mwritev)
{
    mwritev->initialized = true;

    mwritev->fds = new int [Mwritev_MaxFds];
    assert (mwritev->fds);

    mwritev->iovs = new struct iovec* [Mwritev_MaxFds];
    assert (mwritev->iovs);

    mwritev->iovs_heap = new struct iovec [Mwritev_MaxTotalIovs];
    assert (mwritev->iovs_heap);

    mwritev->num_iovs = new int [Mwritev_MaxFds];
    assert (mwritev->num_iovs);

    mwritev->res = new int [Mwritev_MaxFds];
    assert (mwritev->res);
}

} // namespace {}
#endif

Connection::OutputFrontend const DeferredConnectionSender::conn_output_frontend = {
    processOutput
};

mt_unlocks (mutex) void
DeferredConnectionSender::toGlobOutputQueue (bool const add_ref)
{
    if (in_output_queue) {
	mutex.unlock ();
	return;
    }

    in_output_queue = true;
    mutex.unlock ();

    // It is important to ref() before adding the sender to output_queue.
    if (add_ref) {
	Object * const coderef_container = getCoderefContainer();
	if (coderef_container)
	    coderef_container->ref ();
    }

    // TODO Move this to a method of dcs_queue.
    assert (dcs_queue);
    dcs_queue->queue_mutex.lock ();
    dcs_queue->output_queue.append (this);
    dcs_queue->queue_mutex.unlock ();

    dcs_queue->deferred_processor->trigger ();
}

mt_unlocks (mutex) void
DeferredConnectionSender::closeIfNeeded ()
{
    logD (close, fmt_hex, (UintPtr) this, fmt_def, " ", _func,
	  "close_after_flush: ", close_after_flush, ", "
	  "gotDataToSend: ", conn_sender_impl.gotDataToSend());

    if (close_after_flush &&
	!conn_sender_impl.gotDataToSend())
    {
	mutex.unlock ();

	logD (close, _func, "calling frontend->closed");
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
DeferredConnectionSender::sendMessage (MessageEntry * const mt_nonnull msg_entry,
				       bool           const do_flush)
{
    logD (mwritev, _func, "msg_entry: 0x", fmt_hex, (UintPtr) msg_entry);

    mutex.lock ();
    conn_sender_impl.queueMessage (msg_entry);
    if (do_flush) {
	mt_unlocks (mutex) doFlush ();
	return;
    }
    mutex.unlock ();
}

mt_unlocks (mutex) void
DeferredConnectionSender::doFlush ()
{
    if (ready_for_output
	&& conn_sender_impl.gotDataToSend ())
    {
	logD (sender, _func, "calling toGlobOutputQueue()");
	mt_unlocks (mutex) toGlobOutputQueue (true /* add_ref */);
    } else {
	mutex.unlock ();
    }
}

void
DeferredConnectionSender::flush ()
{
    mutex.lock ();
    mt_unlocks (mutex) doFlush ();
}

void
DeferredConnectionSender::closeAfterFlush ()
{
    mutex.lock ();
    logD (close, _func, "in_output_queue: ", in_output_queue);
    close_after_flush = true;
    mt_unlocks (mutex) closeIfNeeded ();
}

DeferredConnectionSender::DeferredConnectionSender (Object * const coderef_container)
    : DependentCodeReferenced (coderef_container),
      dcs_queue (NULL),
      conn_sender_impl (true /* enable_processing_barrier */),
      close_after_flush (false),
      ready_for_output (true),
      in_output_queue (false)
{
    conn_sender_impl.setFrontend (&frontend);
}

DeferredConnectionSender::~DeferredConnectionSender ()
{
//    logD_ (_func, "0x", fmt_hex, (UintPtr) this);

    // Doing lock/unlock to ensure that ~ConnectionSenderImpl() will see correct
    // data.
    mutex.lock();

    // Currently, DeferredConnectionSender cannot be destroyed while it is
    // in output_queue, because it is referenced while it is in the queue.
    assert (!in_output_queue);

    mutex.unlock();
}

bool
DeferredConnectionSenderQueue::process (void *_self)
{
    logD (sender, _func_);

    DeferredConnectionSenderQueue * const self = static_cast <DeferredConnectionSenderQueue*> (_self);

    ProcessingQueue processing_queue;

    self->queue_mutex.lock ();

    if (self->processing) {
	self->queue_mutex.unlock ();
	logW_ (_func, "Concurrent invocation");
	return false;
    }
    self->processing = true;

    {
	OutputQueue::iter iter (self->output_queue);
	while (!self->output_queue.iter_done (iter)) {
	    DeferredConnectionSender * const deferred_sender = self->output_queue.iter_next (iter);
	    // Note that deferred_sender->mutex is not locked here.

	    processing_queue.append (deferred_sender);
	    self->output_queue.remove (deferred_sender);
	}
    }

    self->queue_mutex.unlock ();

    bool extra_iteration_needed = false;

    ProcessingQueue::iter iter (processing_queue);
    while (!processing_queue.iter_done (iter)) {
	DeferredConnectionSender * const deferred_sender = processing_queue.iter_next (iter);
	logD (sender, _func, "deferred_sender: 0x", fmt_hex, (UintPtr) deferred_sender);

	// The only place where 'deferred_sender' may be removed from the queue
	// is its destructor, which won't be called because 'deferred_sender' is
	// refed here.
	deferred_sender->mutex.lock ();

	assert (deferred_sender->in_output_queue);
	deferred_sender->in_output_queue = false;

	// TODO markProcessingBarrier() is useless now, since we check
	// if the barrier has been hit without unlocking the mutex.
	deferred_sender->conn_sender_impl.markProcessingBarrier ();

	AsyncIoResult const res = deferred_sender->conn_sender_impl.sendPendingMessages ();
	if (res == AsyncIoResult::Error ||
	    res == AsyncIoResult::Eof)
	{
	    logD (sender, _func, "res: ", res);

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
	  // deferred_sender from output_queue.

	    mt_unlocks (deferred_sender->mutex) deferred_sender->closeIfNeeded ();

	    Object * const coderef_container = deferred_sender->getCoderefContainer();
	    if (coderef_container)
		coderef_container->unref ();
	} else {
	    // TEST
	    assert (0);

	    mt_unlocks (deferred_sender->mutex) deferred_sender->toGlobOutputQueue (false /* add_ref */);
	    extra_iteration_needed = true;
	}
    }

    self->queue_mutex.lock ();
    if (!self->output_queue.isEmpty())
	extra_iteration_needed = true;

    self->processing = false;
    self->queue_mutex.unlock ();

//    logD_ (_func, "extra_iteration_needed: ", extra_iteration_needed ? "true" : "false");
    return extra_iteration_needed;
}

#ifdef LIBMARY_ENABLE_MWRITEV
bool
DeferredConnectionSenderQueue::process_mwritev (void *_self)
{
    logD (sender, _func_);

    DeferredConnectionSenderQueue * const self = static_cast <DeferredConnectionSenderQueue*> (_self);

    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    LibMary_MwritevData * const mwritev = &tlocal->mwritev;

    ProcessingQueue processing_queue;

    self->queue_mutex.lock ();

    if (!mwritev->initialized)
	mwritevInit (mwritev);

    if (self->processing) {
	self->queue_mutex.unlock ();
	logW_ (_func, "Concurrent invocation");
	return false;
    }
    self->processing = true;

    {
	OutputQueue::iter iter (self->output_queue);
	while (!self->output_queue.iter_done (iter)) {
	    DeferredConnectionSender * const deferred_sender = self->output_queue.iter_next (iter);
	    processing_queue.append (deferred_sender);
	    self->output_queue.remove (deferred_sender);
	}
    }

    self->queue_mutex.unlock ();

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
		if (fd_idx >= Mwritev_MaxFds) {
		    logD_ (_func, "max fds");
		    break;
		}

		DeferredConnectionSender * const deferred_sender = processing_queue.iter_next (iter);

		mwritev->fds [fd_idx] = deferred_sender->conn_sender_impl.getConnection()->getFd();

		deferred_sender->mutex.lock ();

		assert (deferred_sender->in_output_queue);
		deferred_sender->in_output_queue = false;

		deferred_sender->conn_sender_impl.markProcessingBarrier ();

		Count num_iovs = 0;
		deferred_sender->conn_sender_impl.sendPendingMessages_fillIovs (&num_iovs,
										mwritev->iovs_heap + total_iovs,
										Mwritev_MaxIovsPerFd);

		// TODO Try leaving the lock here and unlocking after mwritev() call.
		// TEST (uncomment)
//		deferred_sender->mutex.unlock ();

		mwritev->iovs [fd_idx] = mwritev->iovs_heap + total_iovs;
		mwritev->num_iovs [fd_idx] = num_iovs;

		total_iovs += num_iovs;

		++fd_idx;

		if (Mwritev_MaxTotalIovs - total_iovs < Mwritev_MaxIovsPerFd) {
		    logD_ (_func, "max total iovs");
		    break;
		}
	    }

	    next_iter = iter;
	}

	{
	    Result const res = libMary_mwritev (fd_idx,
						mwritev->fds,
						mwritev->iovs,
						mwritev->num_iovs,
						mwritev->res);
	    if (!res) {
		logE_ (_func, "libMary_mwritev() failed: ", errnoString (errno));
		self->processing = false;
		self->queue_mutex.unlock ();
		return false;
	    }
	}

	fd_idx = 0;
	{
	    ProcessingQueue::iter iter = start_iter;
	    while (!processing_queue.iter_done (iter) &&
		   iter != next_iter)
	    {
		DeferredConnectionSender * const deferred_sender = processing_queue.iter_next (iter);

		bool eintr = false;
		Size num_written = 0;
		int const posix_res = mwritev->res [fd_idx];
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
		    if (async_res == AsyncIoResult::Error) {
			exc_throw <PosixException> (-posix_res);
			logE_ (_func, exc->toString());
		    }

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

		    ++fd_idx;
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

    self->queue_mutex.lock ();
    if (!self->output_queue.isEmpty())
	extra_iteration_needed = true;

    self->processing = false;
    self->queue_mutex.unlock ();

    return extra_iteration_needed;
}
#endif /* LIBMARY_ENABLE_MWRITEV */

mt_const void
DeferredConnectionSenderQueue::setDeferredProcessor (DeferredProcessor * const deferred_processor)
{
    this->deferred_processor = deferred_processor;

#ifdef LIBMARY_ENABLE_MWRITEV
    if (libMary_mwritevAvailable()) {
	send_task.cb = CbDesc<DeferredProcessor::TaskCallback> (
		process_mwritev, this /* cb_data */, getCoderefContainer());
    } else
#endif
    {
	send_task.cb = CbDesc<DeferredProcessor::TaskCallback> (
		process, this /* cb_data */, getCoderefContainer());
    }

    send_reg.setDeferredProcessor (deferred_processor);
    send_reg.scheduleTask (&send_task, true /* permanent */);
}

DeferredConnectionSenderQueue::~DeferredConnectionSenderQueue ()
{
    if (deferred_processor) {
	send_reg.revokeTask (&send_task);
	send_reg.release ();
    }
}

}

