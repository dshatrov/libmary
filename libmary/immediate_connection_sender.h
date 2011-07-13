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


#ifndef __LIBMARY__IMMEDIATE_CONNECTION_SENDER__H__
#define __LIBMARY__IMMEDIATE_CONNECTION_SENDER__H__


#include <libmary/connection.h>
#include <libmary/sender.h>
#include <libmary/connection_sender_impl.h>
#include <libmary/code_referenced.h>


namespace M {

class ImmediateConnectionSender : public Sender,
				  public DependentCodeReferenced
{
private:
  // mt_mutex (mutex) {
    ConnectionSenderImpl conn_sender_impl;

    bool close_after_flush;

    bool ready_for_output;
  // }

    StateMutex mutex;

    void closeIfNeeded ();

    static Connection::OutputFrontend const conn_output_frontend;

    static void processOutput (void *_self);

public:
    void sendMessage (MessageEntry * mt_nonnull msg_entry);

    void flush ();

    void closeAfterFlush ();

    void setConnection (Connection * const mt_nonnull conn)
    {
	conn_sender_impl.setConnection (conn);
	conn->setOutputFrontend (Cb<Connection::OutputFrontend> (&conn_output_frontend, this, getCoderefContainer()));
    }

    ImmediateConnectionSender (Object * const coderef_container)
	: DependentCodeReferenced (coderef_container),
	  close_after_flush (false),
	  ready_for_output (true)
    {
	conn_sender_impl.setFrontend (&frontend);
    }

    ~ImmediateConnectionSender ();
};

}


#endif /* __LIBMARY__IMMEDIATE_CONNECTION_SENDER__H__ */

