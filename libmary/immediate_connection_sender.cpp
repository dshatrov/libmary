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


#include "local_config.h"

#include <libmary/log.h>

#include <libmary/immediate_connection_sender.h>


namespace M {

Connection::OutputFrontend const ImmediateConnectionSender::conn_output_frontend = {
    processOutput
};

// Must be called with 'mutex' held. Releases 'mutex' before returning.
void
ImmediateConnectionSender::closeIfNeeded ()
{
    if (close_after_flush &&
	!conn_sender_impl.gotDataToSend ())
    {
	mutex.unlock ();
	if (frontend && frontend->closed)
	    frontend.call (frontend->closed, /*(*/ (Exception*) NULL /* exc_ */ /*)*/);
    } else {
	mutex.unlock ();
    }
}

void
ImmediateConnectionSender::processOutput (void * const _self)
{
    ImmediateConnectionSender * const self = static_cast <ImmediateConnectionSender*> (_self);

    self->mutex.lock ();
    self->conn_sender_impl.markProcessingBarrier ();
    AsyncIoResult const res = self->conn_sender_impl.sendPendingMessages ();
    if (res == AsyncIoResult::Error ||
	res == AsyncIoResult::Eof)
    {
	self->ready_for_output = false;
	self->mutex.unlock ();

	// exc is NULL for Eof.
	if (res == AsyncIoResult::Error)
	    logE_ (_func, exc->toString());

	if (self->frontend && self->frontend->closed)
	    self->frontend.call (self->frontend->closed, /*(*/ exc /*)*/);
	return;
    }

    if (res == AsyncIoResult::Again)
	self->ready_for_output = false;
    else
	self->ready_for_output = true;

    self->closeIfNeeded ();
    // 'mutex' has been unlocked by closeIfNeeded().
}

void
ImmediateConnectionSender::sendMessage (MessageEntry  * const mt_nonnull msg_entry)
{
    mutex.lock ();
    conn_sender_impl.queueMessage (msg_entry);
    mutex.unlock ();
}

void
ImmediateConnectionSender::flush ()
{
    mutex.lock ();
    if (!ready_for_output) {
	mutex.unlock ();
	return;
    }

    conn_sender_impl.markProcessingBarrier ();
    AsyncIoResult const res = conn_sender_impl.sendPendingMessages ();
    if (res == AsyncIoResult::Error ||
	res == AsyncIoResult::Eof)
    {
	ready_for_output = false;
	mutex.unlock ();
	// TODO It might be better to return Result from flush().
	logE_ (_func, exc->toString());
	if (frontend && frontend->closed)
	    frontend.call (frontend->closed, /*(*/ exc /*)*/);
	return;
    }

    if (res == AsyncIoResult::Again)
	ready_for_output = false;

    closeIfNeeded ();
    // 'mutex' has been unlocked by closeIfNeeded().
}

void
ImmediateConnectionSender::closeAfterFlush ()
{
    mutex.lock ();
    close_after_flush = true;
    // TODO It might be better to return boolean from closeAfterFlush():
    //      if (!conn_sender_impl.gotDataToSend ()) return true;
    closeIfNeeded ();
    // 'mutex' has been unlocked by closeIfNeeded().
}

ImmediateConnectionSender::~ImmediateConnectionSender ()
{
    // Doing lock/unlock to ensure that ~ConnectionSenderImpl() will see correct
    // data.
    mutex.lock();
    mutex.unlock();
}

}

