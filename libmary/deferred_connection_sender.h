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


#include <libmary/libmary_config.h>

#include <libmary/intrusive_list.h>
#include <libmary/connection.h>
#include <libmary/sender.h>
#include <libmary/connection_sender_impl.h>
#include <libmary/code_referenced.h>
#include <libmary/deferred_processor.h>


namespace M {

class DeferredConnectionSenderQueue;

class DeferredConnectionSender_OutputQueue_name;
class DeferredConnectionSender_ProcessingQueue_name;

class DeferredConnectionSender : public Sender,
				 public IntrusiveListElement<DeferredConnectionSender_OutputQueue_name>,
				 public IntrusiveListElement<DeferredConnectionSender_ProcessingQueue_name>,
				 public DependentCodeReferenced
{
    friend class DeferredConnectionSenderQueue;

private:
    mt_const DeferredConnectionSenderQueue *dcs_queue;

  mt_mutex (mutex)
  mt_begin
    ConnectionSenderImpl conn_sender_impl;

    bool close_after_flush;

    bool ready_for_output;

    bool in_output_queue;
  mt_end

    StateMutex mutex;

    mt_unlocks (mutex) void toGlobOutputQueue (bool add_ref);

    mt_unlocks (mutex) void closeIfNeeded ();

    static Connection::OutputFrontend const conn_output_frontend;

    static void processOutput (void *_self);

    mt_unlocks (mutex) void doFlush ();

public:
    void sendMessage (MessageEntry * mt_nonnull msg_entry,
		      bool do_flush = false);

    void flush ();

    void closeAfterFlush ();

    mt_const void setConnection (Connection * const mt_nonnull conn)
    {
	conn_sender_impl.setConnection (conn);
	conn->setOutputFrontend (Cb<Connection::OutputFrontend> (
		&conn_output_frontend, this, getCoderefContainer()));
    }

    mt_const void setQueue (DeferredConnectionSenderQueue * const dcs_queue)
    {
	this->dcs_queue = dcs_queue;
    }

    DeferredConnectionSender (Object *coderef_container);

    ~DeferredConnectionSender ();
};

class DeferredConnectionSenderQueue : public DependentCodeReferenced
{
    friend class DeferredConnectionSender;

private:
    typedef IntrusiveList<DeferredConnectionSender, DeferredConnectionSender_OutputQueue_name> OutputQueue;
    typedef IntrusiveList<DeferredConnectionSender, DeferredConnectionSender_ProcessingQueue_name> ProcessingQueue;

    mt_const DeferredProcessor *deferred_processor;

    DeferredProcessor::Task send_task;
    DeferredProcessor::Registration send_reg;

    mt_mutex (mutex) OutputQueue output_queue;
    mt_mutex (mutex) bool processing;

    Mutex queue_mutex;

    static bool process (void *_self);

#ifdef LIBMARY_ENABLE_MWRITEV
    static bool process_mwritev (void *_self);
#endif

public:
    void setDeferredProcessor (DeferredProcessor * const deferred_processor);

    DeferredConnectionSenderQueue (Object * const coderef_container)
	: DependentCodeReferenced (coderef_container),
	  deferred_processor (NULL),
	  processing (false)
    {
    }

    ~DeferredConnectionSenderQueue ();
};

}


#endif /* __LIBMARY__DEFERRED_CONNECTION_SENDER__H__ */

