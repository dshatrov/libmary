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

#include <libmary/libmary_config.h>
#include <libmary/cb.h>
#include <libmary/exception.h>
#include <libmary/intrusive_list.h>
#include <libmary/page_pool.h>


// M::VSlab is a grow-only data structure. Be warned.
#define LIBMARY_SENDER_VSLAB


#ifdef LIBMARY_SENDER_VSLAB
#include <libmary/vslab.h>
#endif


namespace M {

class Sender
{
public:
    enum SendState {
			      // The comments below describe usual behavior of
			      // user's code when processing sendStateChanged()
			      // notifications.

	ConnectionReady,      // Fast client, no flow control restrictions.

	ConnectionOverloaded, // Slow client, dropping disposable messages.

	QueueSoftLimit,       // Send queue is full, blocking input from
			      // the client as an extra countermeasure.

	QueueHardLimit        // Send queue growth is out of control.
			      // Disconnecting the client.
    };

    struct Frontend {
	// This callback is called with Sender::mutex held, which means that
	// it is forbidden to call any methods of Sender from the callback.
	void (*sendStateChanged) (SendState  send_state,
				  void      *cb_data);

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

#ifdef LIBMARY_SENDER_VSLAB
    static void deleteMessageEntry (MessageEntry * const mt_nonnull msg_entry)
    {
	MessageEntry_Pages * const msg_pages = static_cast <MessageEntry_Pages*> (msg_entry);

	msg_pages->page_pool->msgUnref (msg_pages->first_page);
	if (msg_pages->vslab_key) {
#ifdef LIBMARY_MT_SAFE
	  MutexLock msg_vslab_l (msg_vslab_mutex);
#endif
	    msg_vslab.free (msg_pages->vslab_key);
	} else {
	    delete[] (Byte*) msg_pages;
	}
    }
#else
    static void deleteMessageEntry (MessageEntry * mt_nonnull msg_entry);
#endif

    class MessageEntry_Pages : public MessageEntry
    {
	friend void Sender::deleteMessageEntry (MessageEntry * mt_nonnull msg_entry);
#ifdef LIBMARY_SENDER_VSLAB
	friend class VSlab<MessageEntry_Pages>;
#endif

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

#ifdef LIBMARY_SENDER_VSLAB
	VSlab<MessageEntry_Pages>::AllocKey vslab_key;
#endif

	Byte* getHeaderData () const
	{
	    return (Byte*) this + sizeof (*this);
	}

	static MessageEntry_Pages* createNew (Size const max_header_len)
	{
#ifdef LIBMARY_SENDER_VSLAB
	    unsigned const vslab_header_len = 28;
	    if (max_header_len <= vslab_header_len /* TODO Artificial limit (matches Moment::RtmpConnection's needs) */) {
		VSlab<MessageEntry_Pages>::AllocKey vslab_key;
		MessageEntry_Pages *msg_pages;
		{
#ifdef LIBMARY_MT_SAFE
		  MutexLock msg_vslab_l (msg_vslab_mutex);
#endif
		    msg_pages = msg_vslab.alloc (sizeof (MessageEntry_Pages) + vslab_header_len, &vslab_key);
		}
		msg_pages->vslab_key = vslab_key;
		return msg_pages;
	    } else {
		MessageEntry_Pages * const msg_pages = new (new Byte [sizeof (MessageEntry_Pages) + vslab_header_len]) MessageEntry_Pages;
		msg_pages->vslab_key = NULL;
		return msg_pages;
	    }
#else
	    return new (new Byte [sizeof (MessageEntry_Pages) + max_header_len]) MessageEntry_Pages;
#endif
	}
    };

#ifdef LIBMARY_SENDER_VSLAB
    typedef VSlab<MessageEntry_Pages> MsgVSlab;
    static MsgVSlab msg_vslab;
#ifdef LIBMARY_MT_SAFE
    static Mutex msg_vslab_mutex;
#endif
#endif

protected:
    mt_const Cb<Frontend> frontend;

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

    mt_const void setFrontend (Cb<Frontend> const frontend)
    {
	this->frontend = frontend;
    }
};

}


#endif /* __LIBMARY__SENDER__H__ */

