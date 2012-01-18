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


#ifndef __LIBMARY__HTTP_H__
#define __LIBMARY__HTTP_H__


#include <libmary/types.h>
#include <libmary/hash.h>
#include <libmary/exception.h>
#include <libmary/basic_referenced.h>
#include <libmary/sender.h>
#include <libmary/receiver.h>
#include <libmary/code_referenced.h>
#include <libmary/util_net.h>


namespace M {

class HttpServer;

// Connection -> InputFrontend, OutputFrontend;
// Receiver - только принимает данные (InputFrontend);
// Sender - только отправляет данные (OutputFrontend);
// Server - и принимает, и отправляет данные (Input+Output);
// Service - принимает соединения и сам строит вокруг них цепочки обработки.

// Connection vs Server - ?
// Connection работает ближе к транспортному уровню.
// Server - ближе к бизнес-логике.
// Поэтому RtmpConnection.
// RtmptServer - потому что его нельзя назвать "Connection".

class HttpRequest : public BasicReferenced
{
    friend class HttpServer;

private:
    class Parameter : public HashEntry<>
    {
    public:
	ConstMemory name;
	ConstMemory value;
    };

    typedef Hash< Parameter,
		  ConstMemory,
		  MemberExtractor< Parameter,
				   ConstMemory,
				   &Parameter::name >,
		  MemoryComparator<> >
	    ParameterHash;

    Ref<String> request_line;
    ConstMemory method;
    ConstMemory full_path;
    ConstMemory *path;
    Count num_path_elems;

    Uint64 content_length;

    ParameterHash parameter_hash;

    bool keepalive;

    IpAddress client_addr;

    // TODO Map of get params + iterator over get params, ordered (intrusive list).

public:
    ConstMemory getRequestLine () const
    {
	return request_line->mem();
    }

    ConstMemory getMethod () const
    {
	return method;
    }

    ConstMemory getFullPath () const
    {
	return full_path;
    }

    ConstMemory getPath (Count const index) const
    {
	if (index >= num_path_elems)
	    return ConstMemory();

	return path [index];
    }

    Count getNumPathElems () const
    {
	return num_path_elems;
    }

    Uint64 getContentLength () const
    {
	return content_length;
    }

    // If ret.mem() == NULL, then the parameter is not set.
    // If ret.len() == 0, then the parameter has empty value.
    ConstMemory getParameter (ConstMemory const name)
    {
	Parameter * const param = parameter_hash.lookup (name);
	if (!param)
	    return ConstMemory();

	return param->value;
    }

    void setKeepalive (bool const keepalive)
    {
	this->keepalive = keepalive;
    }

    bool getKeepalive() const
    {
	return keepalive;
    }

    IpAddress getClientAddress () const
    {
	return client_addr;
    }

    void parseParameters (Memory mem);

    HttpRequest ()
	: path (NULL),
	  num_path_elems (0),
	  content_length (0),
	  keepalive (true)
    {
    }

    ~HttpRequest ();
};

class HttpServer : public DependentCodeReferenced
{
public:
    struct Frontend {
	void (*request) (HttpRequest * mt_nonnull req,
			 // TODO ret_parse_query - для разбора данных от форм 
			 void        *cb_data);

	void (*messageBody) (HttpRequest * mt_nonnull req,
			     Memory const &mem,
			     bool         end_of_request,
			     Size        * mt_nonnull ret_accepted,
			     void        *cb_data);

	void (*closed) (Exception *exc,
			void      *cb_data);
    };

private:
    class RequestState
    {
    public:
	enum Value {
	    RequestLine,
	    HeaderField,
	    MessageBody
	};
	operator Value () const { return value; }
	RequestState (Value const value) : value (value) {}
	RequestState () {}
    private:
	Value value;
    };

    Cb<Frontend> frontend;

    Sender *sender;
    PagePool *page_pool;

    IpAddress client_addr;

    Ref<HttpRequest> cur_req;

    RequestState req_state;
    // Number of bytes received for message body / request line / header field.
    Size recv_pos;
    // What the "Content-Length" HTTP header said for the current request.
    Size recv_content_length;

    Result processRequestLine (Memory mem);

    void processHeaderField (ConstMemory const &mem);

    Receiver::ProcessInputResult receiveRequestLine (Memory _mem,
						     Size * mt_nonnull ret_accepted,
						     bool * mt_nonnull ret_header_parsed);

    void resetRequestState ();

  // Receiver frontend.

    static Receiver::Frontend const receiver_frontend;

    static Receiver::ProcessInputResult processInput (Memory const &mem,
						      Size * mt_nonnull ret_accepted,
						      void *_self);

    static void processEof (void *_self);

    static void processError (Exception *exc_,
			      void      *_self);

public:
    // TODO setReceiver

    Cb<Receiver::Frontend> getReceiverFrontend ()
    {
	return Cb<Receiver::Frontend> (&receiver_frontend, this, getCoderefContainer());
    }

    void setFrontend (Cb<Frontend> const frontend)
    {
	this->frontend = frontend;
    }

    void setSender (Sender   * const sender,
		    PagePool * const page_pool)
    {
	this->sender = sender;
	this->page_pool = page_pool;
    }

    void init (IpAddress const &client_addr)
    {
	this->client_addr = client_addr;
    }

    HttpServer (Object * const coderef_container)
	: DependentCodeReferenced (coderef_container),
	  sender (NULL),
	  req_state (RequestState::RequestLine),
	  recv_pos (0),
	  recv_content_length (0)
    {
    }
};

}


#endif /* __LIBMARY__HTTP_H__ */

