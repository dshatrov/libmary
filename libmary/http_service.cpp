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
#include <libmary/util_dev.h>

#include <libmary/http_service.h>


namespace M {

namespace {
LogGroup libMary_logGroup_http_service ("http_service", LogLevel::N);
}

HttpServer::Frontend const HttpService::http_frontend = {
    httpRequest,
    httpMessageBody,
    httpClosed
};

Sender::Frontend const HttpService::sender_frontend = {
    NULL, // sendStateChanged
    senderClosed
};

TcpServer::Frontend const HttpService::tcp_server_frontend = {
    accepted
};

HttpService::HttpConnection::HttpConnection ()
    : valid (true),
      tcp_conn      (this),
      conn_sender   (this),
      conn_receiver (this),
      http_server   (this),
      pollable_key  (NULL),
      conn_keepalive_timer (NULL),
      preassembly_buf (NULL)
{
    logD (http_service, _func, "0x", fmt_hex, (UintPtr) this);
}

HttpService::HttpConnection::~HttpConnection ()
{
    logD (http_service, _func, "0x", fmt_hex, (UintPtr) this);

    if (preassembly_buf)
	delete[] preassembly_buf;
}

mt_mutex (mutex) void
HttpService::releaseHttpConnection (HttpConnection * const mt_nonnull http_conn)
{
    if (http_conn->conn_keepalive_timer) {
	timers->deleteTimer (http_conn->conn_keepalive_timer);
	http_conn->conn_keepalive_timer = NULL;
    }

    poll_group->removePollable (http_conn->pollable_key);
}

void
HttpService::destroyHttpConnection (HttpConnection * const mt_nonnull http_conn)
{
    mutex.lock ();
    if (!http_conn->valid) {
	mutex.unlock ();
	return;
    }
    http_conn->valid = false;

    releaseHttpConnection (http_conn);
    conn_list.remove (http_conn);
    mutex.unlock ();

    http_conn->unref ();
}

void
HttpService::connKeepaliveTimerExpired (void * const _http_conn)
{
    HttpConnection * const http_conn = static_cast <HttpConnection*> (_http_conn);

    logI (http_service, _func, "0x", fmt_hex, (UintPtr) (http_conn));

    CodeRef http_service_ref;
    if (http_conn->weak_http_service.isValid()) {
	http_service_ref = http_conn->weak_http_service;
	if (!http_service_ref)
	    return;
    }
    HttpService * const self = http_conn->unsafe_http_service;

  // TODO FIXME Should httpMessageBody() callback be called here to report
  // an error to the client? See httpClosed() for reference.

    self->destroyHttpConnection (http_conn);
}

void
HttpService::httpRequest (HttpRequest * const mt_nonnull req,
			  void        * const _http_conn)
{
    HttpConnection * const http_conn = static_cast <HttpConnection*> (_http_conn);

    logD (http_service, _func, req->getRequestLine());

    CodeRef http_service_ref;
    if (http_conn->weak_http_service.isValid()) {
	http_service_ref = http_conn->weak_http_service;
	if (!http_service_ref)
	    return;
    }
    HttpService * const self = http_conn->unsafe_http_service;

    http_conn->cur_handler = NULL;
    http_conn->cur_msg_data = NULL;
//    logD_ (_func, "http_conn->cur_handler: 0x", fmt_hex, (UintPtr) http_conn->cur_handler);

    self->mutex.lock ();

    if (self->no_keepalive_conns)
	req->setKeepalive (false);

    if (http_conn->conn_keepalive_timer)
	self->timers->restartTimer (http_conn->conn_keepalive_timer);

  // Searching for a handler with the longest matching path.
  //
  //     /a/b/c/  - last path element should be empty;
  //     /a/b/c   - last path element is "c".

    Namespace *cur_namespace = &self->root_namespace;
    Count const num_path_els = req->getNumPathElems();
//    logD_ (_func, "num_path_els: ", num_path_els);
    ConstMemory handler_path_el;
    for (Count i = 0; i < num_path_els; ++i) {
	ConstMemory const path_el = req->getPath (i);
//	logD_ (_func, "path_el: ", path_el);
	Namespace::NamespaceHash::EntryKey const namespace_key = cur_namespace->namespace_hash.lookup (path_el);
	if (!namespace_key) {
	    handler_path_el = path_el;
	    break;
	}

//	logD_ (_func, "Got namespace key for \"", path_el, "\"");

	cur_namespace = namespace_key.getData();
	assert (cur_namespace);
    }

//    logD_ (_func, "Looking up \"", handler_path_el, "\"");
    Namespace::HandlerHash::EntryKey handler_key = cur_namespace->handler_hash.lookup (handler_path_el);
    if (!handler_key) {
	handler_key = cur_namespace->handler_hash.lookup (ConstMemory());
	// We could add a trailing slash to the path and redirect the client.
	// This would make both "http://a.b/c" and "http://a.b/c/" work.
    }
    if (!handler_key) {
	self->mutex.unlock ();
	logD (http_service, _func, "No suitable handler found");

	ConstMemory const reply_body = "404 Not Found";

	Byte date_buf [timeToString_BufSize];
	Size const date_len = timeToString (Memory::forObject (date_buf), getUnixtime());
	logD (http_service, _func, "page_pool: 0x", fmt_hex, (UintPtr) self->page_pool);
	http_conn->conn_sender.send (
		self->page_pool,
		true /* do_flush */,
		"HTTP/1.1 404 Not found\r\n"
		"Server: Moment/1.0\r\n"
		"Date: ", ConstMemory (date_buf, date_len), "\r\n"
		"Connection: Keep-Alive\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: ", reply_body.len(), "\r\n"
		"\r\n",
		reply_body);
	if (self->no_keepalive_conns)
	  http_conn->conn_sender.closeAfterFlush ();
	return;
    }
    HandlerEntry * const handler = handler_key.getDataPtr();

    // Note: We count on the fact that handler entries are never removed during
    // lifetime of HttpService. This may change in the future, in which case
    // we'll have to add an extra reference to handler entry here.
    self->mutex.unlock ();

    http_conn->cur_handler = handler;
    logD (http_service, _func, "http_conn->cur_handler: 0x", fmt_hex, (UintPtr) http_conn->cur_handler);

    http_conn->preassembled_len = 0;

    if (!handler->preassembly || req->getContentLength() == 0) {
	if (!handler->cb.call (handler->cb->httpRequest,
		    /* ( */ req, &http_conn->conn_sender, Memory(), &http_conn->cur_msg_data /* ) */))
	{
	    http_conn->cur_handler = NULL;
	}
    }
}

void
HttpService::httpMessageBody (HttpRequest  * const mt_nonnull req,
			      Memory const &mem,
			      bool           const end_of_request,
			      Size         * const mt_nonnull ret_accepted,
			      void         * const  _http_conn)
{
    HttpConnection * const http_conn = static_cast <HttpConnection*> (_http_conn);

    if (!http_conn->cur_handler) {
	*ret_accepted = mem.len();
	return;
    }

    if (http_conn->cur_handler->preassembly
	&& http_conn->preassembled_len < http_conn->cur_handler->preassembly_limit)
    {
	Size size = req->getContentLength();
	if (size > http_conn->cur_handler->preassembly_limit)
	    size = http_conn->cur_handler->preassembly_limit;

	bool alloc_new = true;
	if (http_conn->preassembly_buf) {
	    if (http_conn->preassembly_buf_size >= size)
		alloc_new = false;
	    else
		delete[] http_conn->preassembly_buf;
	}

	if (alloc_new) {
	    http_conn->preassembly_buf = new Byte [size];
	    http_conn->preassembly_buf_size = size;
	}

	if (mem.len() + http_conn->preassembled_len < mem.len() /* dirty */
	    || mem.len() + http_conn->preassembled_len >= size)
	{
	    *ret_accepted = 0;
	    if (size > 0) {
		memcpy (http_conn->preassembly_buf + http_conn->preassembled_len,
			mem.mem(),
			size - http_conn->preassembled_len);
		*ret_accepted = size - http_conn->preassembled_len;
		http_conn->preassembled_len = size;

		if (http_conn->cur_handler->parse_body_params) {
//		    logD_ (_func, "Parsing body params:");
//                  logLock ();
//		    hexdump (logs, ConstMemory (http_conn->preassembly_buf, http_conn->preassembled_len));
//                  logUnlock ();
		    req->parseParameters (Memory (http_conn->preassembly_buf, http_conn->preassembled_len));
		}

		if (!http_conn->cur_handler->cb.call (http_conn->cur_handler->cb->httpRequest,
			    /* ( */ req,
				    &http_conn->conn_sender,
				    Memory (http_conn->preassembly_buf, http_conn->preassembled_len),
				    &http_conn->cur_msg_data /* ) */))
		{
		    http_conn->cur_handler = NULL;
		}
	    }

	    if (*ret_accepted < mem.len()) {
		Size accepted = 0;
		if (!http_conn->cur_handler->cb.call (http_conn->cur_handler->cb->httpMessageBody, /*(*/
			    req,
			    &http_conn->conn_sender,
			    mem.region (*ret_accepted),
			    end_of_request,
			    &accepted,
			    &http_conn->cur_msg_data /*)*/)
		    || end_of_request)
		{
		    http_conn->cur_handler = NULL;
		}

		*ret_accepted += accepted;
	    }
	} else {
	    memcpy (http_conn->preassembly_buf + http_conn->preassembled_len,
		    mem.mem(),
		    mem.len());
	    *ret_accepted = mem.len();
	    http_conn->preassembled_len += mem.len();
	}

	return;
    }

    if (!http_conn->cur_handler->cb.call (http_conn->cur_handler->cb->httpMessageBody, /*(*/
		req,
		&http_conn->conn_sender,
		mem,
		end_of_request,
		ret_accepted,
		http_conn->cur_msg_data /*)*/)
	|| end_of_request)
    {
	http_conn->cur_handler = NULL;
	*ret_accepted = mem.len();
    }
}

void
HttpService::doCloseHttpConnection (HttpConnection * const http_conn)
{
    if (http_conn->cur_handler) {
	Size accepted = 0;
	http_conn->cur_handler->cb.call (http_conn->cur_handler->cb->httpMessageBody, /*(*/
		(HttpRequest*) NULL,
		&http_conn->conn_sender,
		Memory(),
		true /* end_of_request */,
		&accepted,
		http_conn->cur_msg_data /*)*/);

	http_conn->cur_handler = NULL;
	logD (http_service, _func, "http_conn->cur_handler: 0x", fmt_hex, (UintPtr) http_conn->cur_handler);
    }

    CodeRef http_service_ref;
    if (http_conn->weak_http_service.isValid()) {
	http_service_ref = http_conn->weak_http_service;
	if (!http_service_ref)
	    return;
    }
    HttpService * const self = http_conn->unsafe_http_service;

    self->destroyHttpConnection (http_conn);
}

void
HttpService::httpClosed (Exception * const exc_,
			 void      * const _http_conn)
{
    HttpConnection * const http_conn = static_cast <HttpConnection*> (_http_conn);

    if (exc_)
	logE_ (_func, exc_->toString());

    logD (http_service, _func, "http_conn 0x", fmt_hex, (UintPtr) http_conn, ", refcount: ", fmt_def, http_conn->getRefCount(), ", cur_handler: 0x", fmt_hex, (UintPtr) http_conn->cur_handler);

    doCloseHttpConnection (http_conn);
}

void
HttpService::senderClosed (Exception * const exc_,
			   void      * const _http_conn)
{
    HttpConnection * const http_conn = static_cast <HttpConnection*> (_http_conn);

    if (exc_)
	logE_ (_func, exc_->toString());

    doCloseHttpConnection (http_conn);
}

bool
HttpService::acceptOneConnection ()
{
    HttpConnection * const http_conn = new HttpConnection;
    assert (http_conn);

    IpAddress client_addr;
    {
	TcpServer::AcceptResult const res = tcp_server.accept (&http_conn->tcp_conn,
							       &client_addr);
	if (res == TcpServer::AcceptResult::Error) {
	    http_conn->unref ();
	    logE_ (_func, exc->toString());
	    return false;
	}

	if (res == TcpServer::AcceptResult::NotAccepted) {
	    http_conn->unref ();
	    return false;
	}

	assert (res == TcpServer::AcceptResult::Accepted);
    }

    http_conn->weak_http_service = this;
    http_conn->unsafe_http_service = this;

    http_conn->cur_handler = NULL;
    http_conn->cur_msg_data = NULL;
    http_conn->preassembly_buf = NULL;
    http_conn->preassembly_buf_size = 0;
    http_conn->preassembled_len = 0;

    http_conn->conn_sender.setConnection (&http_conn->tcp_conn);
    http_conn->conn_sender.setFrontend (CbDesc<Sender::Frontend> (&sender_frontend, http_conn, http_conn));
    http_conn->conn_receiver.setConnection (&http_conn->tcp_conn);
    http_conn->conn_receiver.setFrontend (http_conn->http_server.getReceiverFrontend());

    http_conn->http_server.init (client_addr);
    http_conn->http_server.setSender (&http_conn->conn_sender, page_pool);
    http_conn->http_server.setFrontend (Cb<HttpServer::Frontend> (&http_frontend, http_conn, http_conn));

    mutex.lock ();
    http_conn->pollable_key = poll_group->addPollable (http_conn->tcp_conn.getPollable(), NULL /* ret_reg */);
    if (!http_conn->pollable_key) {
	mutex.unlock ();

	http_conn->unref ();

	logE_ (_func, exc->toString());
	return true;
    }

    if (keepalive_timeout_microsec > 0) {
	// TODO There should be a periodical checker routing which would
	// monitor connection's activity. Currently, this is an overly
	// simplistic oneshot cutter, like a ticking bomb for every client.
	http_conn->conn_keepalive_timer = timers->addTimer (connKeepaliveTimerExpired,
							    http_conn,
							    http_conn,
							    keepalive_timeout_microsec,
							    false /* periodical */);
    } else {
	http_conn->conn_keepalive_timer = NULL;
    }

    conn_list.append (http_conn);
    mutex.unlock ();

    return true;
}

void
HttpService::accepted (void *_self)
{
    HttpService * const self = static_cast <HttpService*> (_self);

    logD (http_service, _func_);

    for (;;) {
	if (!self->acceptOneConnection ())
	    break;
    }
}

mt_mutex (mutex) void
HttpService::addHttpHandler_rec (CbDesc<HttpHandler> const &cb,
				 ConstMemory   const path_,
				 bool          const preassembly,
				 Size          const preassembly_limit,
				 bool          const parse_body_params,
				 Namespace   * const nsp)
{
    ConstMemory path = path_;
    if (path.len() > 0 && path.mem() [0] == '/')
	path = path.region (1);

    Byte const *delim = (Byte const *) memchr (path.mem(), '/', path.len());
    if (!delim) {
	HandlerEntry * const handler_entry = nsp->handler_hash.addEmpty (path).getDataPtr();
	handler_entry->cb = cb;
	handler_entry->preassembly = preassembly;
	handler_entry->preassembly_limit = preassembly_limit;
	handler_entry->parse_body_params = parse_body_params;
	return;
    }

    ConstMemory const next_nsp_name = path.region (0, delim - path.mem());
    Namespace::NamespaceHash::EntryKey const next_nsp_key =
	    nsp->namespace_hash.lookup (next_nsp_name);
    Namespace *next_nsp;
    if (next_nsp_key) {
	next_nsp = next_nsp_key.getData();
    } else {
	Ref<Namespace> const new_nsp = grab (new Namespace);
	nsp->namespace_hash.add (next_nsp_name, new_nsp);
	next_nsp = new_nsp;
    }

    return addHttpHandler_rec (cb,
			       path.region (delim - path.mem() + 1),
			       preassembly,
			       preassembly_limit,
			       parse_body_params,
			       next_nsp);
}

void
HttpService::addHttpHandler (CbDesc<HttpHandler> const &cb,
			     ConstMemory const path,
			     bool        const preassembly,
			     Size        const preassembly_limit,
			     bool        const parse_body_params)
{
//    logD_ (_func, "Adding handler for \"", path, "\"");

    mutex.lock ();
    addHttpHandler_rec (cb,
			path,
			preassembly,
			preassembly_limit,
			parse_body_params,
			&root_namespace); 
    mutex.unlock ();
}

mt_throws Result
HttpService::bind (IpAddress const &addr)
{
    if (!tcp_server.bind (addr))
	return Result::Failure;

    return Result::Success;
}

mt_throws Result
HttpService::start ()
{
    if (!tcp_server.listen ())
	return Result::Failure;

    if (!poll_group->addPollable (tcp_server.getPollable(), NULL /* ret_reg */))
	return Result::Failure;

    return Result::Success;
}

mt_throws Result
HttpService::init (PollGroup * const mt_nonnull poll_group,
		   Timers    * const mt_nonnull timers,
		   PagePool  * const mt_nonnull page_pool,
		   Time        const keepalive_timeout_microsec,
		   bool        const no_keepalive_conns)
{
    this->poll_group = poll_group;
    this->timers = timers;
    this->page_pool = page_pool;
    this->keepalive_timeout_microsec = keepalive_timeout_microsec;
    this->no_keepalive_conns = no_keepalive_conns;

    if (!tcp_server.open ())
	return Result::Failure;

    tcp_server.setFrontend (Cb<TcpServer::Frontend> (&tcp_server_frontend, this, getCoderefContainer()));

    return Result::Success;
}

HttpService::HttpService (Object * const coderef_container)
    : DependentCodeReferenced (coderef_container),
      poll_group (NULL),
      timers (NULL),
      page_pool (NULL),
      keepalive_timeout_microsec (0),
      no_keepalive_conns (false),
      tcp_server (coderef_container)
{
}

HttpService::~HttpService ()
{
  StateMutexLock l (&mutex);

    ConnectionList::iter iter (conn_list);
    while (!conn_list.iter_done (iter)) {
	HttpConnection * const http_conn = conn_list.iter_next (iter);
	releaseHttpConnection (http_conn);
	http_conn->unref ();
    }
}

}

