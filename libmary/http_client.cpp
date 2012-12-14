/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2012 Dmitry Shatrov

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


#include <libmary/http_client.h>
#include <libmary/util_dev.h>


namespace M {

mt_mutex (HttpClient::mutex) void
HttpClient::HttpClientRequest::releaseRequestData ()
{
//    assert (/* not released */);

// No real need to release (overly pedantic).
//            req_path = NULL;
}

HttpClient::HttpClientConnection::HttpClientConnection ()
    : tcp_conn    (this /* coderef_container */),
      sender      (this /* coderef_container */),
      receiver    (this /* coderef_container */),
      http_server (this /* coderef_container */)
{
}

TcpConnection::Frontend const HttpClient::tcp_conn_frontend = {
    connected
};

void
HttpClient::connected (Exception * const exc_,
                       void      * const _http_conn)
{
    HttpClientConnection * const http_conn = static_cast <HttpClientConnection*> (_http_conn);
    HttpClient * const self = http_conn->http_client;

    logD_ (_func_);

    self->mutex.lock ();

    if (exc_) {
        self->destroyHttpClientConnection (http_conn);
        self->mutex.unlock ();
        return;
    }

    List< Ref<HttpClientRequest> >::iter iter (http_conn->requests);
    while (!http_conn->requests.iter_done (iter)) {
        Ref<HttpClientRequest> &http_req = http_conn->requests.iter_next (iter)->data;
        self->sendRequest (http_conn, http_req);
    }

    self->mutex.unlock ();
}

Sender::Frontend const HttpClient::sender_frontend = {
    senderStateChanged,
    senderClosed
};

void
HttpClient::senderStateChanged (Sender::SendState   const /* send_state */,
                                void              * const /* _http_conn */)
{
//    HttpClientConnection * const http_conn = static_cast <HttpClientConnection*> (_http_conn);
//    HttpClient * const self = http_conn->http_client;

    logD_ (_func_);

/*
// TODO This will be covered by reply timeouts.

    if (send_state == Sender::SendState::QueueHardLimit) {
        self->mutex.lock ();
        mt_unlocks_locks (mutex) self->destroyHttpClientConnection (http_conn);
        self->mutex.unlock ();
    }
*/
}

void
HttpClient::senderClosed (Exception * const exc_,
                          void      * const _http_conn)
{
    HttpClientConnection * const http_conn = static_cast <HttpClientConnection*> (_http_conn);
    HttpClient * const self = http_conn->http_client;

    if (exc_)
        logE_ (_func, exc_->toString());

    self->mutex.lock ();
    mt_unlocks_locks (mutex) self->destroyHttpClientConnection (http_conn);
    self->mutex.unlock ();
}

HttpServer::Frontend const HttpClient::http_server_frontend = {
    httpReply,
    httpReplyBody,
    httpClosed
};

void
HttpClient::httpReply (HttpRequest * const mt_nonnull reply,
                       void        *_http_conn)
{
    HttpClientConnection * const http_conn = static_cast <HttpClientConnection*> (_http_conn);

    logD_ (_func_);

  // TODO
}

void
HttpClient::httpReplyBody (HttpRequest  * const mt_nonnull reply,
                           Memory const &mem,
                           bool           const end_of_reply,
                           Size         * const mt_nonnull ret_accepted,
                           void         * const _http_conn)
{
    HttpClientConnection * const http_conn = static_cast <HttpClientConnection*> (_http_conn);

    logD_ (_func,
           "end_of_reply: ", end_of_reply);
    hexdump (logs, mem);

  // TODO

    *ret_accepted = mem.len();
}

void
HttpClient::httpClosed (Exception * const exc_,
                        void      * const _http_conn)
{
    HttpClientConnection * const http_conn = static_cast <HttpClientConnection*> (_http_conn);
    HttpClient * const self = http_conn->http_client;

    logD_ (_func_);

    self->mutex.lock ();
    mt_unlocks_locks (mutex) self->destroyHttpClientConnection (http_conn);
    self->mutex.unlock ();
}

mt_unlocks_locks (mutex) void
HttpClient::destroyHttpClientConnection (HttpClientConnection * const _http_conn)
{
    if (!_http_conn->valid) {
        return;
    }
    _http_conn->valid = false;

    Ref<HttpClientConnection> const http_conn = _http_conn;
    if (http_conn->conn_list_el) {
        http_conns.remove (http_conn->conn_list_el);
        http_conn->conn_list_el = NULL;
    }

    CodeDepRef<ServerThreadContext> const thread_ctx = http_conn->weak_thread_ctx;
    if (thread_ctx) {
        if (http_conn->pollable_key) {
            thread_ctx->getPollGroup()->removePollable (http_conn->pollable_key);
            http_conn->pollable_key = NULL;
        }

        if (!http_conn->tcp_conn.close ())
            logE_ (_func, "tcp_conn.close() failed: ", exc->toString());

      // Manually discarding sender and receiver buffers here might be a good idea.
    }

    {
        List< Ref<HttpClientRequest> >::iter iter (http_conn->requests);
        while (!http_conn->requests.iter_done (iter)) {
            Ref<HttpClientRequest> &http_req = http_conn->requests.iter_next (iter)->data;

            if (!http_req->receiving_body) {
                if (http_req->response_cb) {
                    void *dummy_msg_data = NULL;
                    mt_unlocks_locks (mutex) http_req->response_cb.call_mutex (http_req->response_cb->httpResponse,
                                                                               mutex,
                                                                               /*(*/
                                                                                   (HttpRequest*) NULL /* resp */,
                                                                                   Memory(),
                                                                                   &dummy_msg_data
                                                                               /*)*/);
                    if (!dummy_msg_data)
                        logW_ (_func, "msg_data is likely lost");
                }
            } else {
                if (http_req->response_cb) {
                    Size dummy_accepted = 0;
                    mt_unlocks_locks (mutex) http_req->response_cb.call_mutex (http_req->response_cb->httpResponseBody,
                                                                               mutex,
                                                                               /*(*/
                                                                                   (HttpRequest*) NULL /* resp */,
                                                                                   Memory(),
                                                                                   true /* end_of_response */,
                                                                                   &dummy_accepted,
                                                                                   http_req->user_msg_data
                                                                               /*)*/);
                }
            }

            http_conn->requests.remove (http_req->req_list_el);
        }
        assert (http_conn->requests.isEmpty());
    }
}

mt_mutex (mutex) Ref<HttpClient::HttpClientConnection>
HttpClient::connect (bool * const ret_connected)
{
    *ret_connected = false;

    Ref<HttpClientConnection> const http_conn = grab (new HttpClientConnection);
    http_conn->http_client = this;
    http_conn->server_addr = next_server_addr;
    http_conn->valid = true;
    http_conn->conn_list_el = NULL;

    CodeDepRef<ServerThreadContext> const thread_ctx = server_ctx->selectThreadContext ();
    http_conn->weak_thread_ctx = thread_ctx;

    http_conn->tcp_conn.setFrontend (CbDesc<TcpConnection::Frontend> (&tcp_conn_frontend, http_conn, getCoderefContainer(), http_conn));

    {
        IpAddress client_addr;
        http_conn->http_server.init (client_addr);
    }
    http_conn->http_server.setFrontend (CbDesc<HttpServer::Frontend> (&http_server_frontend, http_conn, getCoderefContainer(), http_conn));
    // Not calling http_conn->http_server.setSender() since we need client mode only.

    http_conn->sender.setConnection (&http_conn->tcp_conn);
    http_conn->sender.setQueue (thread_ctx->getDeferredConnectionSenderQueue());
    http_conn->sender.setFrontend (CbDesc<Sender::Frontend> (&sender_frontend, http_conn, getCoderefContainer(), http_conn));

    http_conn->receiver.setConnection (&http_conn->tcp_conn);
    http_conn->receiver.setFrontend (http_conn->http_server.getReceiverFrontend());

    // TODO FIXME There's a racy nasty problem here: TcpConnection::connected field updates
    // are not synchronized, and we probably want to add pollable before calling connect () - test for this.
    // The same applies to all other invocations of TcpConnection::connect().
    TcpConnection::ConnectResult const connect_res = http_conn->tcp_conn.connect (next_server_addr);
    if (connect_res == TcpConnection::ConnectResult_Error) {
        mutex.unlock ();
        logE_ (_this_func, "http_conn->connect() failed: ", exc->toString());
        return NULL;
    }

    http_conn->pollable_key =
            thread_ctx->getPollGroup()->addPollable (http_conn->tcp_conn.getPollable(),
                                                     NULL /* ret_reg */,
                                                     false /* activate */);

    if (!http_conn->pollable_key) {
        mutex.unlock ();

        logE_ (_func, "addPollable() failed: ", exc->toString());
        return NULL;
    }

    if (!thread_ctx->getPollGroup()->activatePollable (http_conn->pollable_key)) {
        thread_ctx->getPollGroup()->removePollable (http_conn->pollable_key);
        http_conn->pollable_key = NULL;
        mutex.unlock ();

        logE_ (_func, "activatePollable() failed: ", exc->toString());
        return NULL;
    }

    if (connect_res == TcpConnection::ConnectResult_Connected) {
        *ret_connected = true;
    } else {
        assert (connect_res == TcpConnection::ConnectResult_InProgress);
    }

    http_conn->conn_list_el = http_conns.append (http_conn);

    return http_conn;
}

mt_mutex (mutex) Ref<HttpClient::HttpClientConnection>
HttpClient::getConnection (bool * const ret_connected)
{
    *ret_connected = false;

    if (cur_server_addr == next_server_addr
        && keepalive
        && !http_conns.isEmpty())
    {
        *ret_connected = true;
        return http_conns.getLast();
    }

    cur_server_addr = next_server_addr;
    return connect (ret_connected);
}

void
HttpClient::sendRequest (HttpClientConnection * const mt_nonnull http_conn,
                         HttpClientRequest    * const http_req)
{
    // TODO Use 'http_req' request
    http_conn->sender.send (page_pool,
                            true /* do_flush */,
                            "GET /index.html HTTP/1.1\r\n"
                            "Host: momentvideo.org\r\n"
                            "\r\n");

    http_req->releaseRequestData ();
}

Result
HttpClient::queueRequest (HttpRequestType const req_type,
                          ConstMemory     const req_path,
                          CbDesc<HttpResponseHandler> const &response_cb)
{
    mutex.lock ();

    bool connected = false;
    Ref<HttpClientConnection> const http_conn = getConnection (&connected);
    if (!http_conn)
        return Result::Failure;

    Ref<HttpClientRequest> const http_req = grab (new HttpClientRequest);
    http_req->req_type = HttpRequestType_Get;
    // TODO No need to allocate when 'connected' is true.
    http_req->req_path = grab (new String (req_path));
    http_req->response_cb = response_cb;
    http_req->receiving_body = false;
    http_req->user_msg_data = NULL;

    http_req->req_list_el = http_conn->requests.append (http_req);

    if (connected)
        sendRequest (http_conn, http_req);

    mutex.unlock ();

    return Result::Success;
}

Result
HttpClient::httpGet (ConstMemory const req_path,
                     CbDesc<HttpResponseHandler> const &response_cb)
{
    return queueRequest (HttpRequestType_Get, req_path, response_cb);
}

Result
HttpClient::httpPost (ConstMemory const req_path,
                      ConstMemory const post_data,
                      CbDesc<HttpResponseHandler> const &response_cb)
{
    return queueRequest (HttpRequestType_Post, req_path, response_cb);
}

void
HttpClient::setServerAddr (IpAddress const server_addr)
{
    mutex.lock ();
    this->next_server_addr = server_addr;
    mutex.unlock ();
}

mt_const void
HttpClient::init (ServerContext * const mt_nonnull server_ctx,
                  PagePool      * const mt_nonnull page_pool,
                  IpAddress       const server_addr,
                  bool            const keepalive)
{
    this->keepalive = keepalive;
    this->server_ctx = server_ctx;
    this->page_pool = page_pool;
    next_server_addr = server_addr;
}

HttpClient::HttpClient (Object * const coderef_container)
    : DependentCodeReferenced (coderef_container),
      keepalive  (false),
      server_ctx (coderef_container),
      page_pool  (coderef_container)
{
}

HttpClient::~HttpClient ()
{
    mutex.lock ();

    List< Ref<HttpClientConnection> >::iter iter (http_conns);
    while (!http_conns.iter_done (iter)) {
        Ref<HttpClientConnection> &http_conn = http_conns.iter_next (iter)->data;
        mt_unlocks_locks (mutex) destroyHttpClientConnection (http_conn);
    }

    mutex.unlock ();
}

}

