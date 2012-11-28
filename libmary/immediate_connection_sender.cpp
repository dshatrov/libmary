/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2011, 2012 Dmitry Shatrov

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

#include <libmary/immediate_connection_sender.h>


namespace M {

Connection::OutputFrontend const ImmediateConnectionSender::conn_output_frontend = {
    processOutput
};

// Must be called with 'mutex' held. Releases 'mutex' before returning.
mt_unlocks (mutex) void
ImmediateConnectionSender::closeIfNeeded (bool const deferred_event)
{
    if (!closed
        && close_after_flush
	&& !conn_sender_impl.gotDataToSend ())
    {
        closed = true;
	mutex.unlock ();

        if (deferred_event) {
            fireClosed_deferred (&deferred_reg, NULL /* exc_buf */);
            if (frontend) {
                frontend.call_deferred (&deferred_reg,
                                        frontend->closed,
                                        NULL /* extra_ref_data */,
                                        static_cast <Exception*> (NULL) /* exc_ */);
            }
        } else {
            fireClosed (NULL /* exc_ */);
            if (frontend)
                frontend.call (frontend->closed, /*(*/ (Exception*) NULL /* exc_ */ /*)*/);
        }
    } else {
	mutex.unlock ();
    }
}

void
ImmediateConnectionSender::processOutput (void * const _self)
{
    ImmediateConnectionSender * const self = static_cast <ImmediateConnectionSender*> (_self);

    self->mutex.lock ();
    AsyncIoResult const res = self->conn_sender_impl.sendPendingMessages ();
    if (res == AsyncIoResult::Error ||
	res == AsyncIoResult::Eof)
    {
	self->ready_for_output = false;

        bool inform_closed = false;
        if (!self->closed) {
            self->closed = true;
            inform_closed = true;
        }

	self->mutex.unlock ();

	// exc is NULL for Eof.
	if (res == AsyncIoResult::Error) {
	    logE_ (_func, exc->toString());

            if (inform_closed) {
                ExceptionBuffer * const exc_buf = exc_swap_nounref ();

                self->fireClosed (exc_buf->getException());
                if (self->frontend) {
                    self->frontend.call (self->frontend->closed,
                                         /*(*/ exc_buf->getException() /*)*/);
                }

                exc_delete (exc_buf);
            }
        } else {
            if (inform_closed) {
                self->fireClosed (NULL /* exc_ */);
                if (self->frontend) {
                    self->frontend.call (self->frontend->closed,
                                         /*(*/ static_cast <Exception*> (NULL) /* exc_ */ /*)*/);
                }
            }
        }

	return;
    }

    if (res == AsyncIoResult::Again)
	self->ready_for_output = false;
    else
	self->ready_for_output = true;

    mt_unlocks (mutex) self->closeIfNeeded (false /* deferred_event */);
}

void
ImmediateConnectionSender::sendMessage (MessageEntry  * const mt_nonnull msg_entry,
					bool            const do_flush)
{
    mutex.lock ();
    conn_sender_impl.queueMessage (msg_entry);
    if (do_flush) {
	mt_unlocks (mutex) doFlush ();
	return;
    }
    mutex.unlock ();
}

mt_mutex (mutex) mt_unlocks (mutex) void
ImmediateConnectionSender::doFlush ()
{
    if (!ready_for_output) {
	mutex.unlock ();
	return;
    }

    AsyncIoResult const res = conn_sender_impl.sendPendingMessages ();
    if (res == AsyncIoResult::Error ||
	res == AsyncIoResult::Eof)
    {
	ready_for_output = false;

        bool inform_closed = false;
        if (!closed) {
            closed = true;
            inform_closed = true;
        }

	mutex.unlock ();

	// TODO It might be better to return Result from flush().
        //
	// exc is NULL for Eof.
	if (res == AsyncIoResult::Error) {
	    logE_ (_func, exc->toString());

            if (inform_closed) {
                Ref<ExceptionBuffer> const exc_buf = exc_swap ();

                fireClosed_deferred (&deferred_reg, exc_buf);
                if (frontend) {
                    frontend.call_deferred (&deferred_reg,
                                            frontend->closed,
                                            exc_buf /* extra_ref_data */,
                                            exc_buf->getException());
                }
            }
        } else {
            if (inform_closed) {
                fireClosed_deferred (&deferred_reg, NULL /* exc_buf */);
                if (frontend) {
                    frontend.call_deferred (&deferred_reg,
                                            frontend->closed,
                                            NULL /* extra_ref_data */,
                                            static_cast <Exception*> (NULL) /* exc_ */);
                }
            }
        }

	return;
    }

    if (res == AsyncIoResult::Again)
	ready_for_output = false;

    mt_unlocks (mutex) closeIfNeeded (true /* deferred_event */);
}

void
ImmediateConnectionSender::flush ()
{
    mutex.lock ();
    mt_unlocks (mutex) doFlush ();
}

void
ImmediateConnectionSender::closeAfterFlush ()
{
    mutex.lock ();
    close_after_flush = true;
    mt_unlocks (mutex) closeIfNeeded (true /* deferred_event */);
}

void
ImmediateConnectionSender::close ()
{
    mutex.lock ();
    if (closed) {
        mutex.unlock ();
        return;
    }
    closed = true;
    mutex.unlock ();

    fireClosed_deferred (&deferred_reg, NULL /* exc_buf */);
    if (frontend) {
        frontend.call_deferred (&deferred_reg,
                                frontend->closed,
                                NULL /* extra_ref_data */,
                                static_cast <Exception*> (NULL) /* exc_ */);
    }
}

mt_mutex (mutex) bool
ImmediateConnectionSender::isClosed_unlocked ()
{
    return closed;
}

void
ImmediateConnectionSender::lock ()
{
    mutex.lock ();
}

void
ImmediateConnectionSender::unlock ()
{
    mutex.unlock ();
}

ImmediateConnectionSender::ImmediateConnectionSender (Object * const coderef_container)
    : Sender (coderef_container),
      DependentCodeReferenced (coderef_container),
      conn_sender_impl (false /* enable_processing_barrier */),
      closed (false),
      close_after_flush (false),
      ready_for_output (true)
{
    conn_sender_impl.init (&frontend, this /* sender */, &deferred_reg);
}

ImmediateConnectionSender::~ImmediateConnectionSender ()
{
    mutex.lock();
    conn_sender_impl.release ();
    mutex.unlock();
}

}

