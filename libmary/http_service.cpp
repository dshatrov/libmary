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

#include <libmary/http_service.h>


namespace M {

HttpServer::Frontend const
HttpService::http_frontend = {
    httpRequest,
    httpMessageBody,
    httpClosed
};

TcpServer::Frontend const
HttpService::tcp_server_frontend = {
    accepted
};

void
HttpService::releaseHttpConnection (HttpConnection * const mt_nonnull http_conn)
{
    timers->deleteTimer (http_conn->conn_keepalive_timer);
    poll_group->removePollable (http_conn->pollable_key);
}

void
HttpService::connKeepaliveTimerExpired (void * const _http_conn)
{
    HttpConnection * const http_conn = static_cast <HttpConnection*> (_http_conn);

    CodeRef http_service_ref;
    if (http_conn->weak_http_service.isValid()) {
	http_service_ref = http_conn->weak_http_service;
	if (!http_service_ref)
	    return;
    }
    HttpService * const self = http_conn->unsafe_http_service;

    self->releaseHttpConnection (http_conn);

    self->mutex.lock ();
    self->conn_list.remove (http_conn);
    self->mutex.unlock ();

    http_conn->unref ();
}

void
HttpService::httpRequest (HttpRequest * const mt_nonnull req,
			  void        * const _http_conn)
{
    HttpConnection * const http_conn = static_cast <HttpConnection*> (_http_conn);

    CodeRef http_service_ref;
    if (http_conn->weak_http_service.isValid()) {
	http_service_ref = http_conn->weak_http_service;
	if (!http_service_ref)
	    return;
    }
    HttpService * const self = http_conn->unsafe_http_service;

    http_conn->cur_handler = NULL;
    http_conn->cur_msg_data = NULL;

  // Searching for a handler with the longest matching path.
  //
  //     /a/b/c/  - last path element should be empty;
  //     /a/b/c   - last path element is "c".

    self->mutex.lock ();

    Namespace *cur_namespace = &self->root_namespace;
    Count const num_path_els = req->getNumPathElems();
    ConstMemory handler_path_el;
    if (num_path_els > 1) {
	for (Count i = 0; i < num_path_els - 1; ++i) {
	    ConstMemory const path_el = req->getPath (i);
	    Namespace::NamespaceHash::EntryKey const namespace_key = cur_namespace->namespace_hash.lookup (path_el);
	    if (!namespace_key) {
		handler_path_el = req->getPath (i + 1);
		break;
	    }

	    cur_namespace = namespace_key.getData();
	    assert (cur_namespace);
	}
    }

    Namespace::HandlerHash::EntryKey const handler_key = cur_namespace->handler_hash.lookup (handler_path_el);
    if (!handler_key) {
	self->mutex.unlock ();
	return;
    }
    HandlerEntry * const handler = handler_key.getDataPtr();

    // Note: We count on the fact that handler entries are never removed during
    // lifetime of HttpService. This may change in the future, in which case
    // we'll have to add an extra reference to handler entry here.
    self->mutex.unlock ();

    // TODO if (!preassembly), otherwise collect message body up to a specified limit first.
    if (handler->cb.call (handler->cb->httpRequest, /* ( */ req, Memory(), &http_conn->cur_msg_data /* ) */))
	http_conn->cur_handler = handler;
}

void
HttpService::httpMessageBody (HttpRequest * const mt_nonnull req,
			      Memory        const &mem,
			      Size        * const mt_nonnull ret_accepted,
			      void        * const  _http_conn)
{
    HttpConnection * const http_conn = static_cast <HttpConnection*> (_http_conn);

    if (!http_conn->cur_handler)
	return;

#if 0
// Unnecessary
    CodeRef http_service_ref;
    if (http_conn->weak_http_service.isValid()) {
	http_service_ref = http_conn->weak_http_service;
	if (!http_service_ref)
	    return;
    }
    HttpService * const self = http_conn->unsafe_http_service;
#endif

    if (!http_conn->cur_handler->cb.call (http_conn->cur_handler->cb->httpMessageBody,
		/* ( */ req, mem, ret_accepted, http_conn->cur_msg_data /* ) */)
	|| *ret_accepted == mem.len())
    {
	http_conn->cur_handler = NULL;
    }
}

void
HttpService::httpClosed (Exception * const exc_,
			 void      * const _http_conn)
{
    HttpConnection * const http_conn = static_cast <HttpConnection*> (_http_conn);

    if (exc_)
	logE_ (_func, exc_->toString());

    if (!http_conn->cur_handler)
	return;

#if 0
// Unnecessary
    CodeRef http_service_ref;
    if (http_conn->weak_http_service.isValid()) {
	http_service_ref = http_conn->weak_http_service;
	if (!http_service_ref)
	    return;
    }
    HttpService * const self = http_conn->unsafe_http_service;
#endif

    Size accepted = 0;
    http_conn->cur_handler->cb.call (http_conn->cur_handler->cb->httpMessageBody,
	    /* ( */ (HttpRequest*) NULL, Memory(), &accepted, http_conn->cur_msg_data /* ) */);
    http_conn->cur_handler = NULL;
}

bool
HttpService::acceptOneConnection ()
{
    HttpConnection * const http_conn = new HttpConnection;
    assert (http_conn);

    {
	TcpServer::AcceptResult const res = tcp_server.accept (&http_conn->tcp_conn);
	if (res == TcpServer::AcceptResult::Error) {
	    delete http_conn;
	    logE_ (_func, exc->toString());
	    return false;
	}

	if (res == TcpServer::AcceptResult::NotAccepted) {
	    delete http_conn;
	    return false;
	}

	assert (res == TcpServer::AcceptResult::Accepted);
    }

    http_conn->pollable_key = poll_group->addPollable (
	    http_conn->tcp_conn.getPollable());
    if (!http_conn->pollable_key) {
	delete http_conn;
	logE_ (_func, exc->toString());
	return true;
    }

    http_conn->weak_http_service = this;
    http_conn->unsafe_http_service = this;

    http_conn->conn_sender.setConnection (&http_conn->tcp_conn);
    http_conn->conn_receiver.setConnection (&http_conn->tcp_conn);
    http_conn->conn_receiver.setFrontend (http_conn->http_server.getReceiverFrontend());
    http_conn->http_server.setSender (&http_conn->conn_sender, page_pool);
    http_conn->http_server.setFrontend (Cb<HttpServer::Frontend> (&http_frontend, http_conn, http_conn));

    http_conn->conn_keepalive_timer = timers->addTimer (connKeepaliveTimerExpired,
							http_conn,
							http_conn,
							keepalive_timeout_microsec,
							false /* periodical */);

    mutex.lock ();
    conn_list.append (http_conn);
    mutex.unlock ();

    return true;
}

void
HttpService::accepted (void *_self)
{
    HttpService * const self = static_cast <HttpService*> (_self);

    for (;;) {
	if (!self->acceptOneConnection ())
	    break;
    }
}

void
HttpService::addHttpHandler_rec (Cb<HttpHandler>   const &cb,
				 ConstMemory       const &path_,
				 Namespace       * const nsp)
{
    ConstMemory path = path_;
    while (path.len() > 0 && path.mem() [0] == '/')
	path = path.region (1);

    Byte const *delim = (Byte const *) memchr (path.mem(), '/', path.len());
    if (!delim) {
	nsp->handler_hash.add (path, cb);
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

    return addHttpHandler_rec (cb, path.region (delim - path.mem() + 1), next_nsp);
}

void
HttpService::addHttpHandler (Cb<HttpHandler> const &cb,
			     ConstMemory     const &path)
{
    mutex.lock ();
    addHttpHandler_rec (cb, path, &root_namespace); 
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

    if (!poll_group->addPollable (tcp_server.getPollable()))
	return Result::Failure;

    return Result::Success;
}

mt_throws Result
HttpService::init (PollGroup * const mt_nonnull poll_group,
		   Timers    * const mt_nonnull timers,
		   PagePool  * const mt_nonnull,
		   Time        const keepalive_timeout_microsec)
{
    this->poll_group = poll_group;
    this->timers = timers;
    this->page_pool = page_pool;
    this->keepalive_timeout_microsec = keepalive_timeout_microsec;

    return Result::Success;
}

HttpService::HttpService (Object * const coderef_container)
    : DependentCodeReferenced (coderef_container),
      poll_group (NULL),
      timers (NULL),
      page_pool (NULL),
      keepalive_timeout_microsec (0),
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

