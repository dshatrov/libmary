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

#include <libmary/sender.h>


namespace M {

#ifdef LIBMARY_SENDER_VSLAB
Sender::MsgVSlab Sender::msg_vslab (1 << 16 /* prealloc, 64К messages */ /* TODO Preallocate less */);
#ifdef LIBMARY_MT_SAFE
Mutex Sender::msg_vslab_mutex;
#endif
#endif

#ifndef LIBMARY_SENDER_VSLAB
void
Sender::deleteMessageEntry (MessageEntry * const mt_nonnull msg_entry)
{
//    logD_ (_func, "0x", fmt_hex, (UintPtr) msg_entry);

    switch (msg_entry->type) {
	case MessageEntry::Pages: {
	    MessageEntry_Pages * const msg_pages = static_cast <MessageEntry_Pages*> (msg_entry);
	    msg_pages->page_pool->msgUnref (msg_pages->first_page);
	    delete[] (Byte*) msg_pages;
	} break;
	default:
	    unreachable ();
    }
}
#endif

}

