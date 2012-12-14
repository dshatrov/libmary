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


#ifndef __LIBMARY__HTTP_CLIENT__H__
#define __LIBMARY__HTTP_CLIENT__H__


#include <libmary/types.h>
#include <libmary/code_referenced.h>
#include <libmary/object.h>
#include <libmary/tcp_connection.h>
#include <libmary/deferred_connection_sender.h>
#include <libmary/connection_receiver.h>
#include <libmary/http.h>
#include <libmary/server_context.h>


namespace M {

class HttpClient : public DependentCodeReferenced
{
private:
    StateMutex mutex;

public:
    struct HttpResponseHandler
    {
	// If the module has subscribed to request message body pre-assembly,
	// then @msg_body points to message body.
        //
        // If @resp is NULL, then there was an error (no response).
	Result (*httpResponse) (HttpRequest   *resp,
                                Memory const  &msg_body,
                                void         ** mt_nonnull ret_msg_data,
                                void          *cb_data);

	// If @resp is NULL, then we have not received the response in full due to
	// an error. Such last call is made to give the module a chance
	// to release msg_data.
	Result (*httpResponseBody) (HttpRequest  *resp,
				    Memory const &mem,
				    bool          end_of_response,
				    Size         * mt_nonnull ret_accepted,
				    void         *msg_data,
				    void         *cb_data);
    };

private:
    enum HttpRequestType
    {
        HttpRequestType_Get,
        HttpRequestType_Post
    };

    class HttpClientRequest : public Referenced
    {
    public:
        mt_const HttpRequestType req_type;
        mt_const Ref<String> req_path;
        mt_const Cb<HttpResponseHandler> response_cb;

        mt_mutex (HttpClient::Mutex) bool receiving_body;
        mt_mutex (HttpClient::Mutex) List< Ref<HttpClientRequest> >::Element *req_list_el;

        mt_mutex (HttpClient::Mutex) void *user_msg_data;

        void releaseRequestData ();
    };

    class HttpClientConnection : public Object
    {
    public:
        mt_const HttpClient *http_client;
        mt_const IpAddress server_addr;

        mt_const WeakDepRef<ServerThreadContext> weak_thread_ctx;

        mt_mutex (HttpClient::mutex) bool valid;
        mt_mutex (HttpClient::mutex) List< Ref<HttpClientConnection> >::Element *conn_list_el;
        mt_mutex (mutex) PollGroup::PollableKey pollable_key;

        TcpConnection tcp_conn;
        DeferredConnectionSender sender;
        ConnectionReceiver receiver;
        HttpServer http_server;

        mt_mutex (mutex) List< Ref<HttpClientRequest> > requests;

        HttpClientConnection ();
    };

    mt_const bool keepalive;

    mt_const DataDepRef<ServerContext> server_ctx;
    mt_const DataDepRef<PagePool> page_pool;

    mt_mutex (mutex) IpAddress cur_server_addr;
    mt_mutex (mutex) IpAddress next_server_addr;

    mt_mutex (mutex) List< Ref<HttpClientConnection> > http_conns;

  mt_iface (TcpConnection::Frontend)

    static TcpConnection::Frontend const tcp_conn_frontend;

    static void connected (Exception *exc_,
                           void      *_http_conn);

  mt_iface_end

  mt_iface (Sender::Frontend)

    static Sender::Frontend const sender_frontend;

    static void senderStateChanged (Sender::SendState  send_state,
                                    void              *_http_conn);

    static void senderClosed (Exception *exc_,
                              void      *_http_conn);

  mt_iface_end

  mt_iface (HttpServer::Frontend)

    static HttpServer::Frontend const http_server_frontend;

    static void httpReply (HttpRequest * mt_nonnull reply,
                           void        *_http_conn);

    static void httpReplyBody (HttpRequest  * mt_nonnull reply,
                               Memory const &mem,
                               bool          end_of_reply,
                               Size         * mt_nonnull ret_accepted,
                               void         *_http_conn);

    static void httpClosed (Exception *exc_,
                            void      *_http_conn);

  mt_iface_end

    mt_mutex (mutex) void destroyHttpClientConnection (HttpClientConnection *http_conn);

    mt_mutex (mutex) Ref<HttpClientConnection> connect (bool *ret_connected);

    mt_mutex (mutex) Ref<HttpClientConnection> getConnection (bool *ret_connected);

    void sendRequest (HttpClientConnection * mt_nonnull http_conn,
                      HttpClientRequest    *http_req);

    Result queueRequest (HttpRequestType req_type,
                         ConstMemory     req_path,
                         CbDesc<HttpResponseHandler> const &response_cb);

public:
  // TODO Make requests by URI

    Result httpGet (ConstMemory req_path,
                    CbDesc<HttpResponseHandler> const &response_cb);

    Result httpPost (ConstMemory req_path,
                     ConstMemory post_data,
                     CbDesc<HttpResponseHandler> const &response_cb);

    void setServerAddr (IpAddress server_addr);

    mt_const void init (ServerContext * mt_nonnull server_ctx,
                        PagePool      * mt_nonnull page_pool,
                        IpAddress      server_addr,
                        bool           keepalive);

    HttpClient (Object *coderef_container);

    ~HttpClient ();
};

}


#endif /* __LIBMARY__HTTP_CLIENT__H__ */

