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


#ifndef __LIBMARY__DEFERRED_CONNECTION_SENDER__H__
#define __LIBMARY__DEFERRED_CONNECTION_SENDER__H__


#include <libmary/intrusive_list.h>
#include <libmary/connection.h>
#include <libmary/sender.h>
#include <libmary/connection_sender_impl.h>
#include <libmary/code_referenced.h>
#include <libmary/deferred_processor.h>


namespace M {

class DeferredConnectionSender_OutputQueue_name;
class DeferredConnectionSender_ProcessingQueue_name;

class DeferredConnectionSender : public Sender,
				 public IntrusiveListElement<DeferredConnectionSender_OutputQueue_name>,
				 public IntrusiveListElement<DeferredConnectionSender_ProcessingQueue_name>,
				 public DependentCodeReferenced
{
public:
    struct Backend
    {
	// @cb_desc should be called after poll iteration ends.
	void (*trigger) (CbDesc<GenericCallback> const &cb_desc,
			 void *cb_data);
    };

private:
    mt_const Cb<Backend> backend;
    mt_const DeferredProcessor::Registration *deferred_reg;

  mt_mutex (mutex)
  mt_begin
    ConnectionSenderImpl conn_sender_impl;

    bool close_after_flush;

    bool ready_for_output;

    bool in_output_queue;

    typedef IntrusiveList<DeferredConnectionSender, DeferredConnectionSender_OutputQueue_name> OutputQueue;
    typedef IntrusiveList<DeferredConnectionSender, DeferredConnectionSender_ProcessingQueue_name> ProcessingQueue;

    static mt_mutex (glob_output_queue_mutex) OutputQueue glob_output_queue;
    static mt_mutex (glob_output_queue_mutex) bool glob_output_queue_processing;
    static Mutex glob_output_queue_mutex;
  mt_end

    StateMutex mutex;

    mt_mutex (mutex) mt_unlocks (mutex) void toGlobOutputQueue (bool add_ref);

    mt_mutex (mutex) mt_unlocks (mutex) void closeIfNeeded ();

    static Connection::OutputFrontend const conn_output_frontend;

    static void processOutput (void *_self);

public:
    void sendMessage (MessageEntry * mt_nonnull msg_entry);

    void flush ();

    void closeAfterFlush ();

    void setBackend (CbDesc<Backend> const &cb_desc)
    {
	backend = cb_desc;
    }

    void setConnection (Connection * const mt_nonnull conn)
    {
	conn_sender_impl.setConnection (conn);
	conn->setOutputFrontend (Cb<Connection::OutputFrontend> (&conn_output_frontend, this, getCoderefContainer()));
    }

    mt_const void setDeferredRegistration (DeferredProcessor::Registration * const deferred_reg)
    {
	this->deferred_reg = deferred_reg;
    }

    // Call this method once before entering select/epoll syscall every time.
    // Returns 'true' if there's still more data to send.
    static bool pollIterationEnd ();

    DeferredConnectionSender (Object * const coderef_container)
	: DependentCodeReferenced (coderef_container),
	  deferred_reg (NULL),
	  conn_sender_impl (true /* enable_processing_barrier */),
	  close_after_flush (false),
	  ready_for_output (true),
	  in_output_queue (false)
    {
	conn_sender_impl.setFrontend (&frontend);
    }

    ~DeferredConnectionSender ();
};

}


#endif /* __LIBMARY__DEFERRED_CONNECTION_SENDER__H__ */

