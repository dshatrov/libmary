/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2011-2013 Dmitry Shatrov

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


#ifndef LIBMARY__CONNECTION_SENDER_IMPL__H__
#define LIBMARY__CONNECTION_SENDER_IMPL__H__


#include <libmary/libmary_config.h>
#include <libmary/connection.h>
#include <libmary/sender.h>


namespace M {

mt_unsafe class ConnectionSenderImpl
{
private:
    mt_const Cb<Sender::Frontend> *frontend;
    mt_const Sender *sender;
    mt_const DeferredProcessor::Registration *deferred_reg;

    mt_const Connection *conn;

    // Hard queue length limit must be less or equal to soft limit.
    mt_const Count soft_msg_limit;
    mt_const Count hard_msg_limit;

#ifdef LIBMARY_WIN32_IOCP
    Overlapped overlapped;
#endif

    Sender::SendState send_state;
    bool overloaded;

    Sender::MessageList msg_list;
    Count num_msg_entries;

    bool enable_processing_barrier;
    Sender::MessageEntry *processing_barrier;
    bool processing_barrier_hit;

    bool sending_message;

    Size send_header_sent;
    Size send_cur_offset;

    void setSendState (Sender::SendState new_state);

    void resetSendingState ();

    void popPage (Sender::MessageEntry_Pages * mt_nonnull msg_pages);

    mt_throws AsyncIoResult sendPendingMessages_writev ();

    void sendPendingMessages_vector_fill (Count        * mt_nonnull ret_num_iovs,
#ifdef LIBMARY_WIN32_IOCP
                                          WSABUF       * mt_nonnull buffers,
#else
					  struct iovec * mt_nonnull iovs,
#endif
					  Count         num_iovs);

    void sendPendingMessages_vector_react (Count num_iovs);

    void dumpMessage (Sender::MessageEntry * mt_nonnull msg_entry);

public:
    // Takes ownership of msg_entry.
    void queueMessage (Sender::MessageEntry * mt_nonnull msg_entry);

    mt_throws AsyncIoResult sendPendingMessages ();

#ifdef LIBMARY_ENABLE_MWRITEV
    void sendPendingMessages_fillIovs (Count        *ret_num_iovs,
				       struct iovec *ret_iovs,
				       Count         max_iovs);

    void sendPendingMessages_react (AsyncIoResult res,
				    Size          num_written);
#endif

    void markProcessingBarrier () { processing_barrier = msg_list.getLast (); }

    bool processingBarrierHit() const { return processing_barrier_hit; }

    bool gotDataToSend () const
    {
	// TODO msg_list.isEmpty() is enough for this check.
	return sending_message || !msg_list.isEmpty ();
    }

    Sender::SendState getSendState () const { return send_state; }

    mt_const void setConnection (Connection * const conn) { this->conn = conn; }

#ifdef LIBMARY_ENABLE_MWRITEV
    Connection* getConnection () { return conn; }
#endif

    mt_const void init (Cb<Sender::Frontend>            * const frontend,
                        Sender                          * const sender,
                        DeferredProcessor::Registration * const deferred_reg)
    {
        this->frontend = frontend;
        this->sender = sender;
        this->deferred_reg = deferred_reg;
    }

    mt_const void setLimits (Count const soft_msg_limit,
			     Count const hard_msg_limit)
    {
	this->soft_msg_limit = soft_msg_limit;
	this->hard_msg_limit = hard_msg_limit;
    }

    ConnectionSenderImpl (bool enable_processing_barrier);

    void release ();
};

}


#endif /* LIBMARY__CONNECTION_SENDER_IMPL__H__ */

