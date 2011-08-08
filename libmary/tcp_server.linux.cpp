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

#include <libmary/types.h>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include <libmary/log.h>
#include <libmary/posix.h>
#include <libmary/util_net.h>

#include <libmary/tcp_server.h>


namespace M {

PollGroup::Pollable const TcpServer::pollable = {
    processEvents,
    getFd,
    setFeedback
};

void
TcpServer::processEvents (Uint32   const event_flags,
			  void   * const _self)
{
    TcpServer * const self = static_cast <TcpServer*> (_self);

    if (event_flags & PollGroup::Hup)
	logE_ (_func, "PollGroup::Hup");

    if (event_flags & PollGroup::Output)
	logE_ (_func, "PollGroup::Output");

    if (event_flags & PollGroup::Input) {
	if (self->frontend && self->frontend->accepted)
	    self->frontend.call (self->frontend->accepted);
    }

    if (event_flags & PollGroup::Error)
	logE_ (_func, "PollGroup::Error");
}

int
TcpServer::getFd (void *_self)
{
    TcpServer * const self = static_cast <TcpServer*> (_self);
    return self->fd;
}

void
TcpServer::setFeedback (Cb<PollGroup::Feedback> const &feedback,
			void * const _self)
{
    TcpServer * const self = static_cast <TcpServer*> (_self);
    self->feedback = feedback;
}

mt_throws Result
TcpServer::open ()
{
    fd = socket (AF_INET, SOCK_STREAM, 0 /* protocol */);
    if (fd == -1) {
	exc_throw <PosixException> (errno);
	exc_push <InternalException> (InternalException::BackendError);
	logE_ (_func, "socket() failed: ", errnoString (errno));
	return Result::Failure;
    }

    {
	int flags = fcntl (fd, F_GETFL, 0);
	if (flags == -1) {
	    exc_throw <PosixException> (errno);
	    exc_push <InternalException> (InternalException::BackendError);
	    logE_ (_func, "fcntl() failed (F_GETFL): ", errnoString (errno));
	    return Result::Failure;
	}

	flags |= O_NONBLOCK;

	if (fcntl (fd, F_SETFL, flags) == -1) {
	    exc_throw <PosixException> (errno);
	    exc_push <InternalException> (InternalException::BackendError);
	    logE_ (_func, "fcntl() failed (F_SETFL): ", errnoString (errno));
	    return Result::Failure;
	}
    }

    {
	int opt_val = 1;
	int const res = setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof (opt_val));
	if (res == -1) {
	    exc_throw <PosixException> (errno);
	    exc_push <InternalException> (InternalException::BackendError);
	    logE_ (_func, "setsockopt() failed (SO_REUSEADDR): ", errnoString (errno));
	    return Result::Failure;
	} else
	if (res != 0) {
	    exc_throw <InternalException> (InternalException::BackendMalfunction);
	    logE_ (_func, "setsockopt (SO_REUSEADDR): unexpected return value: ", res);
	    return Result::Failure;
	}
    }

    return Result::Success;
}

mt_throws TcpServer::AcceptResult
TcpServer::accept (TcpConnection * const mt_nonnull tcp_connection)
{
    int conn_fd;
    for (;;) {
	// TODO Remember peer's address in TcpConnection
	conn_fd = ::accept (fd, NULL /* addr */, NULL /* addr_len */);
	if (conn_fd == -1) {
	    if (errno == EINTR  ||
		errno == EPROTO ||
		errno == ECONNABORTED)
	    {
	      // Note that we shouldn't return until we're sure that there's
	      // no pending client connections on the server socket (this method
	      // can be though of as it was named "acceptFull" to reflect this).
	      // Otherwise we would loose level-triggered socket state.
		continue;
	    }

	    if (errno == EAGAIN || errno == EWOULDBLOCK) {
		requestInput ();
		return AcceptResult::NotAccepted;
	    }

	    exc_throw <PosixException> (errno);
	    exc_push <InternalException> (InternalException::BackendError);
	    return AcceptResult::Error;
	}

	break;
    }

    {
	int flags = fcntl (conn_fd, F_GETFL, 0);
	if (flags == -1) {
	    exc_throw <PosixException> (errno);
	    exc_push <InternalException> (InternalException::BackendError);
	    logE_ (_func, "fcntl() failed (F_GETFL): ", errnoString (errno));
	    goto _failure;
	}

	flags |= O_NONBLOCK;

	if (fcntl (conn_fd, F_SETFL, flags) == -1) {
	    exc_throw <PosixException> (errno);
	    exc_push <InternalException> (InternalException::BackendError);
	    logE_ (_func, "fcntl() failed (F_SETFL): ", errnoString (errno));
	    goto _failure;
	}
    }

    {
	int opt_val = 1;
	int const res = setsockopt (conn_fd, IPPROTO_TCP, TCP_NODELAY, &opt_val, sizeof (opt_val));
	if (res == -1) {
	    exc_throw <PosixException> (errno);
	    exc_push <InternalException> (InternalException::BackendError);
	    logE_ (_func, "setsockopt() failed (TCP_NODELAY): ", errnoString (errno));
	    goto _failure;
	} else
	if (res != 0) {
	    exc_throw <InternalException> (InternalException::BackendMalfunction);
	    logE_ (_func, "setsockopt() (TCP_NODELAY): unexpected return value: ", res);
	    goto _failure;
	}
    }

    tcp_connection->setFd (conn_fd);

    return AcceptResult::Accepted;

_failure:
    for (;;) {
	int const res = ::close (conn_fd);
	if (res == -1) {
	    if (errno == EINTR)
		continue;

	    logE_ (_func, "close() failed: ", errnoString (errno));
	} else
	if (res != 0) {
	    logE_ (_func, "close(): unexpected return value: ", res);
	}

	break;
    }

    return AcceptResult::Error;
}

mt_throws Result
TcpServer::bind (IpAddress const &ip_addr)
{
    struct sockaddr_in addr;
    setIpAddress (ip_addr, &addr);

    int const res = ::bind (fd, (struct sockaddr*) &addr, sizeof (addr));
    if (res == -1) {
	exc_throw <PosixException> (errno);
	exc_push <InternalException> (InternalException::BackendError);
	logE_ (_func, "bind() failed: ", errnoString (errno));
	return Result::Failure;
    } else
    if (res != 0) {
	exc_throw <InternalException> (InternalException::BackendMalfunction);
	logE_ (_func, "bind(): unexpected return value: ", res);
	return Result::Failure;
    }

    return Result::Success;
}

mt_throws Result
TcpServer::listen ()
{
    int const res = ::listen (fd, SOMAXCONN);
    if (res == -1) {
	exc_throw <PosixException> (errno);
	exc_push <InternalException> (InternalException::BackendError);
	logE_ (_func, "listen() failed: ", errnoString (errno));
	return Result::Failure;
    } else
    if (res != 0) {
	exc_throw <InternalException> (InternalException::BackendMalfunction);
	logE_ (_func, "listen(): unexpected return value: ", res);
	return Result::Failure;
    }

    return Result::Success;
}

mt_throws Result
TcpServer::close ()
{
    Result ret_res = Result::Success;

    if (fd != -1) {
	for (;;) {
	    int const res = ::close (fd);
	    if (res == -1) {
		if (errno == EINTR)
		    continue;

		exc_throw <PosixException> (errno);
		logE_ (_func, "close() failed: ", errnoString (errno));
		ret_res = Result::Failure;
	    } else
	    if (res != 0) {
		exc_throw <InternalException> (InternalException::BackendMalfunction);
		logE_ (_func, "close(): unexpected return value: ", res);
		ret_res = Result::Failure;
	    }

	    break;
	}

	fd = -1;
    } else {
	logW_ (_func, "not opened");
    }

    return ret_res;
}

TcpServer::~TcpServer ()
{
    if (fd != -1) {
	for (;;) {
	    int const res = ::close (fd);
	    if (res == -1) {
		if (errno == EINTR)
		    continue;

		logE_ (_func, "close() failed: ", errnoString (errno));
	    } else
	    if (res != 0) {
		logE_ (_func, "close(): unexpected return value: ", res);
	    }

	    break;
	}
    }
}

}

