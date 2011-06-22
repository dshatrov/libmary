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


namespace M {

class DeferredConnectionSender_OutputQueue_name;

class DeferredConnectionSender : public Sender,
				 public IntrusiveListElement<DeferredConnectionSender_OutputQueue_name>,
				 public DependentCodeReferenced
{
private:
  // mt_mutex (mutex)
  // {
    ConnectionSenderImpl conn_sender_impl;

    bool close_after_flush;

    bool in_output_queue;

    typedef IntrusiveList<DeferredConnectionSender, DeferredConnectionSender_OutputQueue_name> OutputQueue;

    static OutputQueue glob_output_queue;
    static Mutex glob_output_queue_mutex;

  //}

    StateMutex mutex;

    mt_mutex (mutex) void toGlobOutputQueue ();

    mt_mutex (mutex) mt_unlocks void closeIfNeeded ();

    static Connection::OutputFrontend const conn_output_frontend;

    static void processOutput (void *_self);

public:
    void sendMessage (MessageEntry * mt_nonnull msg_entry);

    void flush ();

    void closeAfterFlush ();

    // TODO conn_sender->setWritevFd()

    void setConnection (Connection * const mt_nonnull conn)
    {
	conn_sender_impl.setConnection (conn);
	conn->setOutputFrontend (Cb<Connection::OutputFrontend> (&conn_output_frontend, this, getCoderefContainer()));
    }

    // Call this method once before entering select/epoll syscall every time.
    static void pollIterationEnd ();

    DeferredConnectionSender (Object * const coderef_container)
	: DependentCodeReferenced (coderef_container),
	  close_after_flush (false),
	  in_output_queue (false)
    {
    }

    ~DeferredConnectionSender ();
};

}


#endif /* __LIBMARY__DEFERRED_CONNECTION_SENDER__H__ */

