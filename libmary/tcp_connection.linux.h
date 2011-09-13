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


#ifndef __LIBMARY__TCP_CONNECTION__LINUX__H__
#define __LIBMARY__TCP_CONNECTION__LINUX__H__


#include <libmary/libmary_config.h>
#include <libmary/code_referenced.h>
#include <libmary/connection.h>
#include <libmary/poll_group.h>
#include <libmary/debug.h>


namespace M {

class TcpConnection : public Connection,
		      public DependentCodeReferenced
{
public:
    struct Frontend {
	void (*connected) (Exception *exc_ mt_exc_kind ((IoException, InternalException)),
			   void *cb_data);
    };

#ifdef LIBMARY_TCP_CONNECTION_NUM_INSTANCES
    static AtomicInt num_instances;
#endif

private:
    int fd;
    // Must be accessed only from processEvents() or during initialization
    // (by connect()).
    bool connected;

    // Synchronized by processEvents() and also used by read(). This implies
    // that read() must be called from the same thread as processEvents(),
    // which is not very pleasant.
    bool hup_received;

    Cb<Frontend> frontend;
    Cb<PollGroup::Feedback> feedback;

    void requestInput ()
    {
	if (feedback && feedback->requestInput)
	    feedback.call (feedback->requestInput);
    }

    void requestOutput ()
    {
	if (feedback && feedback->requestOutput)
	    feedback.call (feedback->requestOutput);
    }

  mt_iface (PollGroup::Pollable)

    static PollGroup::Pollable const pollable;

    static void processEvents (Uint32  event_flags,
			       void   *_self);

    static int getFd (void *_self);

    static void setFeedback (Cb<PollGroup::Feedback> const &feedback,
			     void *_self);

  mt_iface_end(PollGroup::Pollable)

public:
  mt_iface (Connection)

    mt_iface (AsyncInputStream)

      mt_throws AsyncIoResult read (Memory const &mem,
				    Size         *ret_nread);

    mt_iface_end (AsyncInputStream)

    mt_iface (AsyncOutputStream)

      mt_throws AsyncIoResult write (ConstMemory const &mem,
				     Size *ret_nwritten);

      mt_throws AsyncIoResult writev (struct iovec *iovs,
				      Count         num_iovs,
				      Size         *ret_nwritten);

    mt_iface_end (AsyncOutputStream)

    // Note that close() closes the file descriptor, which may cause races
    // if the connection object is still in use, i.e. it is referenced and
    // read/write methods may potentially be called.
    mt_throws Result close ();

#ifdef LIBMARY_ENABLE_MWRITEV
    int getFd ()
    {
	return fd;
    }
#endif

  mt_iface_end (Connection)

    void setFrontend (Cb<Frontend> const &frontend)
    {
	this->frontend = frontend;
    }

    CbDesc<PollGroup::Pollable> getPollable ()
    {
	return CbDesc<PollGroup::Pollable> (&pollable, this, getCoderefContainer());
    }

    // May be called only once. Must be called early (during initialzation)
    // to ensure proper synchronization of accesses to 'connected' data member.
    mt_throws Result connect (IpAddress const &ip_addr);


    // Should be called just once by TcpServer.
    void setFd (int const fd)
    {
	this->fd = fd;
	connected = true;
    }

    TcpConnection (Object *coderef_container);

    ~TcpConnection ();
};

}


#endif /* __LIBMARY__TCP_CONNECTION__LINUX__H__ */

