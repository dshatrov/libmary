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


#ifndef __LIBMARY__CONNECTION_RECEIVER__H__
#define __LIBMARY__CONNECTION_RECEIVER__H__


#include <libmary/receiver.h>
#include <libmary/connection.h>
#include <libmary/code_referenced.h>


namespace M {

class ConnectionReceiver : public Receiver,
			   public DependentCodeReferenced
{
private:
    Connection *conn;

    Byte *recv_buf;
    Size const recv_buf_len;

    Size recv_buf_pos;
    Size recv_accepted_pos;

    void doProcessInput ();

    static Connection::InputFrontend const conn_input_frontend;

    static void processInput (void *_self);

    static void processError (Exception *exc_,
			      void *_self);

public:
    void setConnection (Connection * const conn)
    {
	this->conn = conn;
	conn->setInputFrontend (Cb<Connection::InputFrontend> (&conn_input_frontend, this, getCoderefContainer()));
    }

    // TODO Deprecated constructor. Delete it.
    ConnectionReceiver (Object     * const coderef_container,
			Connection * const mt_nonnull conn);

    ConnectionReceiver (Object * const coderef_container);

    ~ConnectionReceiver ();
};

}


#endif /* __LIBMARY__CONNECTION_RECEIVER__H__ */

