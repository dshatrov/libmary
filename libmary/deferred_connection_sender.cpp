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


#include <libmary/log.h>

#include <libmary/deferred_connection_sender.h>


namespace M {

DeferredConnectionSender::OutputQueue DeferredConnectionSender::glob_output_queue;
bool DeferredConnectionSender::glob_output_queue_processing = false;
Mutex DeferredConnectionSender::glob_output_queue_mutex;

Connection::OutputFrontend const DeferredConnectionSender::conn_output_frontend = {
    processOutput
};

mt_mutex (mutex) void
DeferredConnectionSender::toGlobOutputQueue ()
{
    if (in_output_queue)
	return;

    in_output_queue = true;
    glob_output_queue_mutex.lock ();
    glob_output_queue.append (this);
    glob_output_queue_mutex.unlock ();
    {
	Object * const coderef_container = getCoderefContainer();
	if (coderef_container)
	    coderef_container->ref ();
    }
}

mt_mutex (mutex) mt_unlocks bool
DeferredConnectionSender::closeIfNeeded ()
{
    if (close_after_flush &&
	!conn_sender_impl.gotDataToSend ())
    {
	if (frontend && frontend->closed) {
	    mutex.unlock ();
	    frontend.call (frontend->closed, /*(*/ (Exception*) NULL /* exc_ */);
	}

	return true;
    } else {
	mutex.unlock ();
    }

    return false;
}

void
DeferredConnectionSender::processOutput (void * const _self)
{
    DeferredConnectionSender * const self = static_cast <DeferredConnectionSender*> (_self);
    self->mutex.lock ();
    self->ready_for_output = true;
    if (self->conn_sender_impl.gotDataToSend ()) {
	self->toGlobOutputQueue ();
    }
    self->mutex.unlock ();
}

void
DeferredConnectionSender::sendMessage (MessageEntry * const mt_nonnull msg_entry)
{
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
	toGlobOutputQueue ();
    }
    mutex.unlock ();
}

void
DeferredConnectionSender::closeAfterFlush ()
{
    mutex.lock ();
    close_after_flush = true;
    closeIfNeeded ();
    // 'mutex' has been unlocked by closeIfNeeded().
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
	    deferred_sender->conn_sender_impl.markProcessingBarrier ();
	    processing_queue.append (deferred_sender);

	    glob_output_queue.remove (deferred_sender);
	    deferred_sender->in_output_queue = false;
	}
    }

    glob_output_queue_mutex.unlock ();

    bool extra_iteration_needed = false;

    ProcessingQueue::iter iter (processing_queue);
    while (!processing_queue.iter_done (iter)) {
#if 0
// Deprecated.
    for (;;) {
	glob_output_queue_mutex.lock ();
#error TODO Use DeferredProcessor to avoid starvation.
	DeferredConnectionSender * const deferred_sender = glob_output_queue.getFirst ();
	if (!deferred_sender) {
	    glob_output_queue_mutex.unlock ();
	    break;
	}
	glob_output_queue.remove (deferred_sender);
	glob_output_queue_mutex.unlock ();
#endif
	DeferredConnectionSender * const deferred_sender = processing_queue.iter_next (iter);

	// The only place where 'deferred_sender' may be removed from the queue
	// is its destructor, which won't be called because 'deferred_sender' is
	// refed here.
	deferred_sender->mutex.lock ();

#if 0
// Deprecated.
	assert (deferred_sender->in_output_queue);
	deferred_sender->in_output_queue = false;
#endif

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

	// vvv FIXME Wrong comment because of extra_iteartion_needed vvv
	//
	// At this point, conn_sender_impl has either sent all data, or it has
	// gotten EAGAIN from writev. In either case, we should have removed
	// deferred_sender from glob_output_queue.

	extra_iteration_needed = deferred_sender->conn_sender_impl.processingBarrierHit();
//	logD_ (_func, "extra_iteration_needed: ", extra_iteration_needed ? "true" : "false");

	if (deferred_sender->closeIfNeeded ())
	    extra_iteration_needed = false;

	// 'deferred_sender->mutex' has been unlocked by closeIfNeeded().
	if (!extra_iteration_needed) {
	    Object * const coderef_container = deferred_sender->getCoderefContainer();
	    if (coderef_container)
		coderef_container->unref ();
	}
    }

    glob_output_queue_mutex.lock ();
    glob_output_queue_processing = false;
    glob_output_queue_mutex.unlock ();

//    logD_ (_func, "extra_iteration_needed: ", extra_iteration_needed ? "true" : "false");
    return extra_iteration_needed;
}

DeferredConnectionSender::~DeferredConnectionSender ()
{
    // Doing lock/unlock to ensure that ~ConnectionSenderImpl() will see correct
    // data.
    mutex.lock();

#if 0
// Deprecated.
    // Removing the sender from glob_output_queue if it is in the queue.
    if (in_output_queue) {
	glob_output_queue_mutex.lock ();
	glob_output_queue.remove (this);
	glob_output_queue_mutex.unlock ();
    }
#endif
    // Currently, DeferredConnectionSender cannot be destroyed while it is
    // in glob_output_queue, because it is referenced while it is in the queue.
    assert (!in_output_queue);

    mutex.unlock();
}

}

