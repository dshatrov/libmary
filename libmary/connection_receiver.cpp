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


#include <libmary/log.h>

#include <libmary/connection_receiver.h>


namespace M {

namespace {
LogGroup libMary_logGroup_msg ("msg", LogLevel::N);
}

Connection::InputFrontend const ConnectionReceiver::conn_input_frontend = {
    processInput,
    processError
};

void
ConnectionReceiver::doProcessInput ()
{
    logD (msg, _func_);

    for (;;) {
	assert (recv_buf_pos < recv_buf_len);
	Size const toread = recv_buf_len - recv_buf_pos;
	if (toread == 0) {
	    logW_ (_func, "recieve buffer is full");
	    return;
	}

	Size nread;
	AsyncIoResult io_res;
	{
	    io_res = conn->read (Memory (recv_buf + recv_buf_pos, toread), &nread);
	    logD (msg, _func, "read(): ", io_res);
	    switch (io_res) {
		case AsyncIoResult::Again: {
		    // TODO if (recv_buf_pos >= recv_buf_len) then error.
		    return;
		} break;
		case AsyncIoResult::Error: {
		    logD_ (_func, "read() failed: ", exc->toString());
		    if (!error_reported) {
			error_reported = true;
			if (frontend && frontend->processError)
			    frontend.call (frontend->processError, /*(*/ exc /*)*/);
		    }
		    return;
		} break;
		case AsyncIoResult::Eof: {
		    if (frontend && frontend->processEof)
			frontend.call (frontend->processEof);
		    return;
		} break;
		case AsyncIoResult::Normal:
		case AsyncIoResult::Normal_Again:
		case AsyncIoResult::Normal_Eof:
		  // No-op
		    break;
		default:
		    unreachable ();
	    }
	}
	assert (nread <= toread);
	recv_buf_pos += nread;

	logD (msg, _func, "nread: ", nread, ", recv_accepted_pos: ", recv_accepted_pos, ", recv_buf_pos: ", recv_buf_pos);

//	if (recv_buf_pos >= recv_needed_len)
	assert (recv_accepted_pos <= recv_buf_pos);
	Size const toprocess = recv_buf_pos - recv_accepted_pos;
	Size num_accepted;
	ProcessInputResult res;
	if (!frontend.call_ret<ProcessInputResult> (&res, frontend->processInput, /*(*/
		     Memory (recv_buf + recv_accepted_pos, toprocess),
		     &num_accepted /*)*/))
	{
	    num_accepted = 0;
	    // TODO In MomentVideo, this does nothing.
	    res = ProcessInputResult::Error;
	}
	assert (num_accepted <= toprocess);
	logD (msg, _func, res);
	switch (res) {
	    case ProcessInputResult::Normal:
		assert (num_accepted == toprocess);
		recv_buf_pos = 0;
		recv_accepted_pos = 0;
		break;
	    case ProcessInputResult::Error:
		logD_ (_func, "user's input handler failed");
		if (!error_reported) {
		    error_reported = true;
		    if (frontend && frontend->processError) {
			InternalException internal_exc (InternalException::FrontendError);
			frontend.call (frontend->processError, /*(*/ &internal_exc /*)*/);
		    }
		}
		return;
	    case ProcessInputResult::Again:
		recv_accepted_pos += num_accepted;
		if (recv_accepted_pos > 0) {
		    if (recv_buf_len - recv_accepted_pos <= (recv_buf_len >> 1)) {
			logD (msg, _func, "shifting receive buffer, toprocess: ", toprocess, ", num_accepted: ", num_accepted);
			memcpy (recv_buf, recv_buf + recv_accepted_pos, toprocess - num_accepted);
			recv_buf_pos = toprocess - num_accepted;
			recv_accepted_pos = 0;
		    }
		}
		// If the buffer is full and the frontend wants more data, then
		// we fail to serve the client. This should never happen with
		// properly written frontends.
		if (recv_buf_pos >= recv_buf_len) {
		    logE_ (_func, "Read buffer is full, frontend should have consumed some data. "
			   "recv_accepted_pos: ", recv_accepted_pos, ", "
			   "recv_buf_pos: ", recv_buf_pos, ", "
			   "recv_buf_len: ", recv_buf_len);
		    unreachable ();
		}
		break;
	    case ProcessInputResult::InputBlocked:
		recv_accepted_pos += num_accepted;
		// TODO block input
		return;
	    default:
		unreachable ();
	}

	if (io_res == AsyncIoResult::Normal_Again)
	    return;

	if (io_res == AsyncIoResult::Normal_Eof) {
	    if (frontend && frontend->processEof)
		frontend.call (frontend->processEof);
	    return;
	}
    } // for (;;)
}

void
ConnectionReceiver::processInput (void * const _self)
{
    ConnectionReceiver * const self = static_cast <ConnectionReceiver*> (_self);
    self->doProcessInput ();
}

void
ConnectionReceiver::processError (Exception * const exc_,
				  void      * const _self)
{
//    logD_ (_func_);
    ConnectionReceiver * const self = static_cast <ConnectionReceiver*> (_self);

    if (self->error_reported) {
	return;
    }
    self->error_reported = true;

    if (self->frontend && self->frontend->processError)
	self->frontend.call (self->frontend->processError, /*(*/ exc_ /*)*/);
}

// TODO Deprecated constructor.
ConnectionReceiver::ConnectionReceiver (Object     * const coderef_container,
					Connection * const mt_nonnull conn)
    : DependentCodeReferenced (coderef_container),
      conn (conn),
      recv_buf_len (1 << 16 /* 64 Kb */),
      recv_buf_pos (0),
      recv_accepted_pos (0),
      error_reported (false)
{
    recv_buf = new Byte [recv_buf_len];
    assert (recv_buf);

    conn->setInputFrontend (Cb<Connection::InputFrontend> (&conn_input_frontend, this, getCoderefContainer()));
}

ConnectionReceiver::ConnectionReceiver (Object * const coderef_container)
    : DependentCodeReferenced (coderef_container),
      recv_buf_len (1 << 16 /* 64 Kb */),
      recv_buf_pos (0),
      recv_accepted_pos (0),
      error_reported (false)
{
    recv_buf = new Byte [recv_buf_len];
    assert (recv_buf);
}

ConnectionReceiver::~ConnectionReceiver ()
{
    delete[] recv_buf;
}

}

