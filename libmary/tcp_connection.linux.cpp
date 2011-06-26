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


#include <libmary/types.h>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <libmary/log.h>
#include <libmary/posix.h>
#include <libmary/util_net.h>

#include <libmary/tcp_connection.h>


namespace M {

PollGroup::Pollable const TcpConnection::pollable = {
    processEvents,
    getFd,
    setFeedback
};

void
TcpConnection::processEvents (Uint32   const event_flags,
			      void   * const _self)
{
    TcpConnection * const self = static_cast <TcpConnection*> (_self);

//    logD_ (_func, fmt_hex, event_flags);
//    logD_ (_func, "Connection::frontend: 0x", fmt_hex, (uintptr_t) Connection::frontend);

    if (event_flags & PollGroup::Hup) {
      // TODO remember eof
	logD_ (_func, "Hup");
    }

    if (event_flags & PollGroup::Output) {
	if (!self->connected) {
	    self->connected = true;

	    int opt_val = 0;
	    socklen_t opt_len = sizeof (opt_val);
	    int const res = getsockopt (self->fd, SOL_SOCKET, SO_ERROR, &opt_val, &opt_len);
	    if (res == -1) {
		int const errnum = errno;

		logE_ (_func, "getsockopt() failed: ", errnoString (errno));

		PosixException posix_exc (errnum);
		InternalException internal_exc (InternalException::BackendError);
		internal_exc.cause = &posix_exc;

		if (self->frontend && self->frontend->connected)
		    self->frontend.call (self->frontend->connected, /*(*/ &internal_exc /*)*/);

		if (self->input_frontend && self->input_frontend->processError)
		    self->input_frontend.call (self->input_frontend->processError, /*(*/ &internal_exc /*)*/);

		return;
	    } else
	    if (res != 0) {
		logE_ (_func, "getsockopt(): unexpected return value: ", res);

		InternalException internal_exc (InternalException::BackendMalfunction);

		if (self->frontend && self->frontend->connected)
		    self->frontend.call (self->frontend->connected, /*(*/ &internal_exc /*)*/);

		if (self->input_frontend && self->input_frontend->processError)
		    self->input_frontend.call (self->input_frontend->processError, /*(*/ &internal_exc /*)*/);

		return;
	    }

	    if (opt_val == 0) {
		if (self->frontend && self->frontend->connected)
		    self->frontend.call (self->frontend->connected,  /*(*/ (Exception*) NULL /* exc */ /*)*/);
	    } else {
		if (opt_val != EINPROGRESS && opt_val != EINTR) {
		    logE_ (_func, "connection error: ", errnoString (opt_val));

		    PosixException posix_exc (opt_val);
		    InternalException internal_exc (InternalException::BackendError);
		    internal_exc.cause = &posix_exc;

		    if (self->frontend && self->frontend->connected)
			self->frontend.call (self->frontend->connected, /*(*/ &internal_exc /*)*/);

		    if (self->input_frontend && self->input_frontend->processError)
			self->input_frontend.call (self->input_frontend->processError, /*(*/ &internal_exc /*)*/);

		    return;
		} else {
		    logW_ (_func, "got output event, but not connected yet. opt_val: ", opt_val);
		    return;
		}
	    }
	}

	if (self->output_frontend && self->output_frontend->processOutput)
	    self->output_frontend.call (self->output_frontend->processOutput);
    }

    if (event_flags & PollGroup::Input) {
	if (self->input_frontend && self->input_frontend->processInput)
	    self->input_frontend.call (self->input_frontend->processInput);
    }

    if (event_flags & PollGroup::Error) {
	logD_ (_func, "Error");
	if (self->input_frontend && self->input_frontend->processError) {
	    // TODO getsockopt SO_ERROR + fill PosixException
	    IoException io_exc;
	    self->input_frontend.call (self->input_frontend->processError, /*(*/ &io_exc /*)*/);
	}
    }

    if (!(event_flags & PollGroup::Input)  &&
	!(event_flags & PollGroup::Output) &&
	!(event_flags & PollGroup::Error)  &&
	!(event_flags & PollGroup::Hup))
    {
	logD_ (_func, "no events, this: 0x", fmt_hex, (UintPtr) self);
	return;
    }
}

int
TcpConnection::getFd (void *_self)
{
    TcpConnection * const self = static_cast <TcpConnection*> (_self);
    return self->fd;
}

void
TcpConnection::setFeedback (Cb<PollGroup::Feedback> const &feedback,
			    void * const _self)
{
    TcpConnection * const self = static_cast <TcpConnection*> (_self);
    self->feedback = feedback;
}

AsyncIoResult
TcpConnection::read (Memory const &mem,
		     Size * const ret_nread,
		     bool * const ret_eof)
    mt_throw ((IoException,
	       InternalException))
{
    if (ret_nread)
	*ret_nread = 0;
    if (ret_eof)
	*ret_eof = false;

    Size len;
    if (mem.len() > SSIZE_MAX)
	len = SSIZE_MAX;
    else
	len = mem.len();

    ssize_t const res = recv (fd, mem.mem(), (ssize_t) len, 0 /* flags */);
    if (res == -1) {
	if (errno == EAGAIN || errno == EWOULDBLOCK) {
	    requestInput ();
	    return AsyncIoResult::Again;
	}

	if (errno == EINTR)
	    return AsyncIoResult::Normal;

	exc_throw <PosixException> (errno);
	exc_push <IoException> ();
	return AsyncIoResult::Error;
    } else
    if (res < 0) {
	exc_throw <InternalException> (InternalException::BackendMalfunction);
	return AsyncIoResult::Error;
    } else
    if (res == 0) {
	if (ret_eof)
	    *ret_eof = true;

	return AsyncIoResult::Eof;
    }

    if (ret_nread)
	*ret_nread = (Size) res;

    return AsyncIoResult::Normal;
}

AsyncIoResult
TcpConnection::write (ConstMemory const &mem,
		      Size * const ret_nwritten)
    mt_throw ((IoException,
	       InternalException))
{
    if (ret_nwritten)
	*ret_nwritten = 0;

    Size len;
    if (mem.len() > SSIZE_MAX)
	len = SSIZE_MAX;
    else
	len = mem.len();

    ssize_t const res = send (fd, mem.mem(), (ssize_t) len, MSG_NOSIGNAL);
    if (res == -1) {
	if (errno == EAGAIN || errno == EWOULDBLOCK) {
	    requestOutput ();
	    return AsyncIoResult::Again;
	}

	if (errno == EINTR)
	    return AsyncIoResult::Normal;

	if (errno == EPIPE)
	    return AsyncIoResult::Eof;

	exc_throw <PosixException> (errno);
	exc_push <IoException> ();
	return AsyncIoResult::Error;
    } else
    if (res < 0) {
	exc_throw <InternalException> (InternalException::BackendMalfunction);
	return AsyncIoResult::Error;
    }

    if (ret_nwritten)
	*ret_nwritten = (Size) res;

    return AsyncIoResult::Normal;
}

mt_throws AsyncIoResult
TcpConnection::writev (struct iovec * const iovs,
		       Count          const num_iovs,
		       Size         * const ret_nwritten)
{
    if (ret_nwritten)
	*ret_nwritten = 0;

    ssize_t const res = ::writev (fd, iovs, num_iovs);
    if (res == -1) {
	if (errno == EAGAIN || errno == EWOULDBLOCK) {
	    requestOutput ();
	    return AsyncIoResult::Again;
	}

	if (errno == EINTR)
	    return AsyncIoResult::Normal;

	if (errno == EPIPE)
	    return AsyncIoResult::Eof;

	exc_throw <PosixException> (errno);
	exc_push <InternalException> (InternalException::BackendError);
	logE_ (_func, "writev() failed: ", errnoString (errno));

	logE_ (_func, "num_iovs: ", num_iovs);
	for (Count i = 0; i < num_iovs; ++i) {
	    logE_ (_func, "iovs[", i, "]: ", fmt_hex, (UintPtr) iovs [i].iov_base, ", ", fmt_def, iovs [i].iov_len);
	}

	return AsyncIoResult::Error;
    } else
    if (res < 0) {
	exc_throw <InternalException> (InternalException::BackendMalfunction);
	logE_ (_func, "writev(): unexpected return value: ", res);
	return AsyncIoResult::Error;
    }

    if (ret_nwritten)
	*ret_nwritten = (Size) res;

    return AsyncIoResult::Normal;
}

mt_throws Result
TcpConnection::close ()
{
    if (fd == -1)
	return Result::Success;

    for (;;) {
	int const res = ::close (fd);
	if (res == -1) {
	    if (errno == EINTR)
		continue;

	    exc_throw <PosixException> (errno);
	    logE_ (_func, "close() failed: ", errnoString (errno));
	    return Result::Failure;
	} else
	if (res != 0) {
	    exc_throw <InternalException> (InternalException::BackendError);
	    logE_ (_func, "close(): unexpected return value: ", res);
	    return Result::Failure;
	}

	break;
    }

    return Result::Success;
}

mt_throws Result
TcpConnection::connect (IpAddress const &addr)
{
    struct sockaddr_in saddr;
    setIpAddress (addr, &saddr);

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
	int const res = setsockopt (fd, IPPROTO_TCP, TCP_NODELAY, &opt_val, sizeof (opt_val));
	if (res == -1) {
	    exc_throw <PosixException> (errno);
	    exc_push <InternalException> (InternalException::BackendError);
	    logE_ (_func, "setsockopt() failed (TCP_NODELAY): ", errnoString (errno));
	    return Result::Failure;
	} else
	if (res != 0) {
	    exc_throw <InternalException> (InternalException::BackendMalfunction);
	    logE_ (_func, "setsockopt() (TCP_NODELAY): unexpected return value: ", res);
	    return Result::Failure;
	}
    }

    for (;;) {
	int const res = ::connect (fd, (struct sockaddr*) &saddr, sizeof (saddr));
	if (res == 0) {
	    connected = true;
	    if (frontend && frontend->connected) {
		logD_ (_func, "Calling frontend->connected");
		frontend.call (frontend->connected, /*(*/ (Exception*) NULL /* exc */ /*)*/);
	    }
	} else
	if (res == -1) {
	    if (errno == EINTR)
		continue;

	    if (errno == EINPROGRESS) {
		logD_ (_func, "EINPROGRESS");
		break;
	    }

	    exc_throw <PosixException> (errno);
	    exc_push <IoException> ();
	    logE_ (_func, "connect() failed: ", errnoString (errno));
	    return Result::Failure;
	} else {
	    exc_throw <InternalException> (InternalException::BackendMalfunction);
	    logE_ (_func, "connect(): unexpected return value ", res);
	    return Result::Failure;
	}

	break;
    }

    return Result::Success;
}

TcpConnection::~TcpConnection ()
{
    logD_ (_func, "0x", fmt_hex, (UintPtr) this);

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

