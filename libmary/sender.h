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


#ifndef __LIBMARY__SENDER__H__
#define __LIBMARY__SENDER__H__


#include <libmary/types.h>

#include <new>

#include <libmary/cb.h>
#include <libmary/exception.h>
#include <libmary/intrusive_list.h>
#include <libmary/page_pool.h>


namespace M {

class Sender
{
public:
    struct Frontend {
	void (*closed) (Exception *exc_,
			void      *cb_data);
    };

    class MessageList_name;

    class MessageEntry : public IntrusiveListElement<MessageList_name>
    {
    public:
	enum Type {
	    Pages
	};

	Type const type;

	MessageEntry (Type const type)
	    : type (type)
	{
	}
    };

    typedef IntrusiveList <MessageEntry, MessageList_name> MessageList;

    static void deleteMessageEntry (MessageEntry * mt_nonnull msg_entry);

    class MessageEntry_Pages : public MessageEntry
    {
	friend void Sender::deleteMessageEntry (MessageEntry * mt_nonnull msg_entry);

    private:
	MessageEntry_Pages ()
	    : MessageEntry (MessageEntry::Pages)
	{
	}

	~MessageEntry_Pages ()
	{
	}

    public:
	Size header_len;

	PagePool *page_pool;
	PagePool::Page *first_page;
	Size msg_offset;

	Byte* getHeaderData () const
	{
	    return (Byte*) this + sizeof (*this);
	}

	static MessageEntry_Pages* createNew (Size const max_header_len)
	{
	    return new (new Byte [sizeof (MessageEntry_Pages) + max_header_len]) MessageEntry_Pages;
	}
    };

protected:
    Cb<Frontend> frontend;

public:
    // Takes ownership of msg_entry.
    virtual void sendMessage (MessageEntry * mt_nonnull msg_entry) = 0;

    virtual void flush () = 0;

    // Frontend::closed() will be called after message queue becomes empty.
    virtual void closeAfterFlush () = 0;

    void sendPages (PagePool               * const mt_nonnull page_pool,
		    PagePool::PageListHead * const mt_nonnull page_list)
    {
	MessageEntry_Pages * const msg_pages = MessageEntry_Pages::createNew (0 /* max_header_len */);
	msg_pages->header_len = 0;
	msg_pages->page_pool = page_pool;
	msg_pages->first_page = page_list->first;
	msg_pages->msg_offset = 0;

	sendMessage (msg_pages);
    }

    template <class ...Args>
    void send (PagePool * const mt_nonnull page_pool,
	       Args const &...args)
    {
	PagePool::PageListHead page_list;
	page_pool->printToPages (&page_list, args...);

	MessageEntry_Pages * const msg_pages = MessageEntry_Pages::createNew (0 /* max_header_len */);
	msg_pages->header_len = 0;
	msg_pages->page_pool = page_pool;
	msg_pages->first_page = page_list.first;
	msg_pages->msg_offset = 0;

	sendMessage (msg_pages);
    }

    void setFrontend (Cb<Frontend> const frontend)
    {
	this->frontend = frontend;
    }
};

}


#endif /* __LIBMARY__SENDER__H__ */

