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

mt_mutex (mutex) mt_unlocks void
DeferredConnectionSender::closeIfNeeded ()
{
    if (close_after_flush &&
	!conn_sender_impl.gotDataToSend ())
    {
	if (frontend && frontend->closed) {
	    mutex.unlock ();
	    frontend.call (frontend->closed, /*(*/ (Exception*) NULL /* exc_ */);
	}
    } else {
	mutex.unlock ();
    }
}

void
DeferredConnectionSender::processOutput (void * const _self)
{
    DeferredConnectionSender * const self = static_cast <DeferredConnectionSender*> (_self);
    self->mutex.lock ();
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
    if (conn_sender_impl.gotDataToSend ()) {
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

void
DeferredConnectionSender::pollIterationEnd ()
{
  // Processing global output queue.
    for (;;) {
	glob_output_queue_mutex.lock ();
	DeferredConnectionSender * const deferred_sender = glob_output_queue.getFirst ();
	if (!deferred_sender) {
	    glob_output_queue_mutex.unlock ();
	    break;
	}
	glob_output_queue.remove (deferred_sender);
	glob_output_queue_mutex.unlock ();

	// The only place where 'deferred_sender' may be removed from the queue
	// is its destructor, which won't be called because 'deferred_sender is
	// refed here.
	deferred_sender->mutex.lock ();
	assert (deferred_sender->in_output_queue);
	deferred_sender->in_output_queue = false;

	if (!deferred_sender->conn_sender_impl.sendPendingMessages ()) {
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

	deferred_sender->closeIfNeeded ();
	// 'deferred_sender->mutex' has been unlocked by closeIfNeeded().
	{
	    Object * const coderef_container = deferred_sender->getCoderefContainer();
	    if (coderef_container)
		coderef_container->unref ();
	}
    }
}

DeferredConnectionSender::~DeferredConnectionSender ()
{
    // Doing lock/unlock to ensure that ~ConnectionSenderImpl() will see correct
    // data.
    mutex.lock();
    // Removing the sender from glob_output_queue if it is in the queue.
    if (in_output_queue) {
	glob_output_queue_mutex.lock ();
	glob_output_queue.remove (this);
	glob_output_queue_mutex.unlock ();
    }
    mutex.unlock();
}

}

