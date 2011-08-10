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

