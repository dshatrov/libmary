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
#include <libmary/util_dev.h>
#include <libmary/log.h>

#include <libmary/connection_sender_impl.h>


namespace M {

namespace {
LogGroup libMary_logGroup_send    ("send",    LogLevel::N);
LogGroup libMary_logGroup_writev  ("writev",  LogLevel::N);
LogGroup libMary_logGroup_close   ("close",   LogLevel::D);
LogGroup libMary_logGroup_hexdump ("hexdump", LogLevel::N);
}

void
ConnectionSenderImpl::setSendState (Sender::SendState const new_state)
{
    if (new_state == send_state)
	return;

    send_state = new_state;
    if (frontend
	&& (*frontend)
	&& (*frontend)->sendStateChanged)
    {
	frontend->call ((*frontend)->sendStateChanged, /* ( */ new_state /* ) */);
    }
}

void
ConnectionSenderImpl::resetSendingState ()
{
  // We get here every time a complete message has been send.

    if (msg_list.isEmpty ()) {
	sending_message = false;
	// Note that if there was 'closeAfterFlush()' for ConnectionSenderImpl,
	// then this would be the right place to signal "closed" if message
	// queue is empty.
	return;
    }

    sending_message = true;
    send_header_sent = 0;

    Sender::MessageEntry * const cur_msg = msg_list.getFirst();
#if 0
// TODO msg_len may be uninitialized for sendRaw
    if (cur_msg->msg_len < out_chunk_size)
	send_chunk_left = cur_msg->msg_len;
    else
	send_chunk_left = out_chunk_size;
#endif

    // XXX MessageEntry type dependency
    if (cur_msg->type == Sender::MessageEntry::Pages) {
	Sender::MessageEntry_Pages * const msg_pages = static_cast <Sender::MessageEntry_Pages*> (cur_msg);
	send_cur_offset = msg_pages->msg_offset;
	logD (send, _func, "msg_pages->msg_offset: ", msg_pages->msg_offset);
    } else {
	send_cur_offset = 0;
    }
    logD (send, _func, "send_cur_offset: ", send_cur_offset);
}

void
ConnectionSenderImpl::popPage (Sender::MessageEntry_Pages * const mt_nonnull msg_pages)
{
    PagePool::Page * const next_page = msg_pages->first_page->getNextMsgPage ();
    logD (send, _func, "unrefing page 0x", fmt_hex, (UintPtr) msg_pages->first_page);
    msg_pages->page_pool->pageUnref (msg_pages->first_page);
    msg_pages->first_page = next_page;
    msg_pages->msg_offset = 0;
    send_cur_offset = 0;
}

AsyncIoResult
ConnectionSenderImpl::sendPendingMessages ()
    mt_throw ((IoException,
	       InternalException))
{
    logD (send, _func_);

    processing_barrier_hit = false;

    if (!sending_message) {
	if (!msg_list.getFirst())
	    return AsyncIoResult::Normal;

	logD (send, _func, "calling resetSendingState()");
	resetSendingState ();
    }

    logD (send, _func, "calling sendPendingMessages_writev()");
    return sendPendingMessages_writev ();
}

AsyncIoResult
ConnectionSenderImpl::sendPendingMessages_writev ()
    mt_throw ((IoException,
	       InternalException))
{
    for (;;) {
	if (!gotDataToSend()) {
	    logD (send, _func, "no data to send");

	    if (send_state == Sender::ConnectionOverloaded)
		setSendState (Sender::ConnectionReady);

	    overloaded = false;
	    return AsyncIoResult::Normal;
	}

	if (processingBarrierHit()) {
	    logD (send, _func, "processing barrier hit");
	    return AsyncIoResult::Normal;
	}

	// TODO Count num_iovs
	Size num_iovs = 0;
#if 0
// Сейчас не используем режим count_iovs, и просто выделяем на стеке массив
// длиной IOV_MAX.
	sendPendingMessages_vector (true  /* count_iovs */,
				    false /* fill_iovs */,
				    false /* react */,
				    &num_iovs,
				    NULL  /* iovs */,
				    0     /* num_iovs */,
				    0     /* num_written */);
#endif

	// Note: Comments in boost headers tell that posix platforms are not
	// required to define IOV_MAX.
	struct iovec iovs [IOV_MAX];
	sendPendingMessages_vector (false /* count_iovs */,
				    true  /* fill_iovs */,
				    false /* react */,
				    &num_iovs,
				    iovs,
// WRONG				    (Count) -1 /* num_iovs */,
				    IOV_MAX /* num_iovs */,
				    0     /* num_written */);
	if (num_iovs > IOV_MAX) {
	    logE_ (_func, "num_iovs: ", num_iovs, ", IOV_MAX: ", IOV_MAX);
	    assert (0);
	}
//	if (num_iovs < IOV_MAX)
//	    logD_ (_func, "< IOV_MAX");

#if 0
	// Dump of all iovs.
	if (defaultLogLevelOn (LogLevel::Debug)) {
	    logD (writev, _func, "iovs:");
	    for (Count i = 0; i < num_iovs; ++i) {
		logD (writev, "    #", i, ": 0x", (UintPtr) iovs [i].iov_base, ": ", iovs [i].iov_len);
		hexdump (logs, ConstMemory ((Byte const *) iovs [i].iov_base, iovs [i].iov_len));
	    }
	}
#endif

	Size num_written = 0;
	{
	    bool tmp_processing_barrier_hit = processing_barrier_hit;
	    processing_barrier_hit = false;

	    AsyncIoResult const res = conn->writev (iovs, num_iovs, &num_written);
	    if (res == AsyncIoResult::Again) {
		if (send_state == Sender::ConnectionReady)
		    setSendState (Sender::ConnectionOverloaded);

		logD (send, _func, "connection overloaded");

		overloaded = true;
		return AsyncIoResult::Again;
	    }

	    if (res == AsyncIoResult::Error)
		return AsyncIoResult::Error;

	    if (res == AsyncIoResult::Eof) {
		logD (close, _func, "Eof, num_iovs: ", num_iovs);
		return AsyncIoResult::Eof;
	    }

	    processing_barrier_hit = tmp_processing_barrier_hit;

	    // Normal_Again is not handled specially here yet.

	    // Note that we can get "num_written == 0" here in two cases:
	    //   a) writev() syscall returned EINTR;
	    //   b) there was nothing to send.
	    // That's why we do not do a usual EINTR loop here.
	}

	sendPendingMessages_vector (false /* count_iovs */,
				    false /* fill_iovs */,
				    true  /* react */,
				    &num_iovs,
				    NULL  /* iovs */,
				    num_iovs,
				    num_written);
    } // for (;;)

    unreachable();
    return AsyncIoResult::Normal;
}

// @count_iovs   - Сосчитать кол-во векторов, нужных для отправки всех сообщений в очереди.
//                 Подсчёт останавливается при достижении лимита IOV_MAX;
// @fill_iovs    - Заполнить массив векторов (@iovs, @num_iovs);
// @react        - Обновить состояние отправки сообщений в зависимости от кол-ва байт,
//                 успешно записанных последним вызовом writev() (@num_written);
// @ret_num_iovs - Кол-во векторов. Подсчитывается, если @count_iovs истинно;
// @iovs         - Массив векторов для заполнения;
// @num_iovs     - Размер массива векторов для заполнения;
// @num_written  - Кол-во байт, успешно записанных последним вызовом writev().
//
void
ConnectionSenderImpl::sendPendingMessages_vector (bool           const count_iovs,
						  bool           const fill_iovs,
						  bool           const react,
						  Count        * const ret_num_iovs,
						  struct iovec * const iovs,
						  Count          const num_iovs,
						  Size                 num_written)
{
    logD (writev, _func, count_iovs ? "count " : "", fill_iovs ? "fill " : "", react ? "react " : "");

    Sender::MessageEntry *msg_entry = msg_list.getFirst ();
    if (!msg_entry) {
	logD (writev, _func, "message queue is empty");
	return;
    }

    if (enable_processing_barrier
	&& !processing_barrier)
    {
	if (gotDataToSend())
	    processing_barrier_hit = true;

	logD (writev, _func, "processing barrier is NULL");
	return;
    }

    // Valid if @count_iovs is false.
    Count cur_num_iovs = 0;

    *ret_num_iovs = 0;

    bool first_entry = true;
    Count i = 0;
    while (msg_entry) {
	Sender::MessageEntry * const next_msg_entry = msg_list.getNext (msg_entry);

	// If still set to 'true' after the switch, then msg_entry is removed
	// from the queue.
	bool msg_sent_completely = true;
	switch (msg_entry->type) {
#if 0
	    case Sender::MessageEntry::Buffer: {
	      // TODO
		unreachable ();
	    } break;
#endif
	    case Sender::MessageEntry::Pages: {
		logD (writev, _func, "message entry");

		Sender::MessageEntry_Pages * const msg_pages = static_cast <Sender::MessageEntry_Pages*> (msg_entry);

		// Разделение "if (first_entry) {} else {}" нужно, потому что переменные
		// состояния send*, в том числе и send_header_sent, обновляются только
		// при последнем вызове этого метода (в фазе "react").
		if (first_entry) {
		    if (send_header_sent < msg_pages->header_len) {
			logD (writev, _func, "first entry, header");

			if (count_iovs) {
			    ++*ret_num_iovs;
			    if (*ret_num_iovs >= IOV_MAX)
				break;
			} else {
			    ++cur_num_iovs;
			    if (cur_num_iovs > num_iovs)
				break;
			    ++*ret_num_iovs;
			}

			if (fill_iovs) {
			    iovs [i].iov_base = msg_pages->getHeaderData() + send_header_sent;
			    iovs [i].iov_len = msg_pages->header_len - send_header_sent;
			    ++i;
			}

			if (react) {
			    if (num_written < msg_pages->header_len - send_header_sent) {
				send_header_sent += num_written;
				num_written = 0;
				msg_sent_completely = false;
				break;
			    }

			    num_written -= msg_pages->header_len - send_header_sent;
			    send_header_sent = msg_pages->header_len;
			}
		    }
		} else {
		    if (msg_pages->header_len > 0) {
			logD (writev, _func, "header");

			if (count_iovs) {
			    ++*ret_num_iovs;
			    if (*ret_num_iovs >= IOV_MAX)
				break;
			} else {
			    ++cur_num_iovs;
			    if (cur_num_iovs > num_iovs)
				break;
			    ++*ret_num_iovs;
			}

			if (fill_iovs) {
			    iovs [i].iov_base = msg_pages->getHeaderData();
			    iovs [i].iov_len = msg_pages->header_len;
			    ++i;
			}

			if (react) {
			    if (num_written < msg_pages->header_len) {
				send_header_sent = num_written;
				num_written = 0;
				msg_sent_completely = false;
				break;
			    }

			    send_header_sent = msg_pages->header_len;
			    num_written -= msg_pages->header_len;
			}
		    }
		} // if (first_entry)

		PagePool::Page *page = msg_pages->first_page;
		bool first_page = true;
		while (page) {
		    logD (writev, _func, "page");

		    PagePool::Page * const next_page = page->getNextMsgPage();

		    if (page->data_len > 0) {
			logD (writev, _func, "non-empty page");

			// FIXME Limit on the number of iovs conflicts with send barrier logics.
			//       There probably are bugs because of this.
			//       Consider situations where we hit num_iovs limit and the message
			//       has been sent completely.
			if (count_iovs) {
			    ++*ret_num_iovs;
			    if (*ret_num_iovs >= IOV_MAX)
				break;
			} else {
			    ++cur_num_iovs;
			    if (cur_num_iovs > num_iovs)
				break;
			    ++*ret_num_iovs;
			}

			if (fill_iovs) {
			    if (first_page) {
				if (first_entry) {
				    logD (writev, _func, "#", i, ": first page, first entry");

				    assert (send_cur_offset < page->data_len);
				    iovs [i].iov_base = page->getData() + send_cur_offset;
				    iovs [i].iov_len = page->data_len - send_cur_offset;
				} else {
				    logD (writev, _func, "#", i, ": first page");

				    assert (msg_pages->msg_offset < page->data_len);
				    iovs [i].iov_base = page->getData() + msg_pages->msg_offset;
				    iovs [i].iov_len = page->data_len - msg_pages->msg_offset;
				}
			    } else {
				logD (writev, _func, "#", i);

				iovs [i].iov_base = page->getData();
				iovs [i].iov_len = page->data_len;
			    }

			    ++i;
			}

			if (react) {
			    if (first_page) {
				if (first_entry) {
				    assert (send_cur_offset < page->data_len);
				    if (num_written < page->data_len - send_cur_offset) {
					send_cur_offset += num_written;
					num_written = 0;
					msg_sent_completely = false;
					break;
				    }

				    num_written -= page->data_len - send_cur_offset;
				} else {
				    assert (msg_pages->msg_offset < page->data_len);
				    if (num_written < page->data_len - msg_pages->msg_offset) {
					send_cur_offset += num_written;
					num_written = 0;
					msg_sent_completely = false;
					break;
				    }

				    num_written -= page->data_len - msg_pages->msg_offset;
				}
			    } else {
				if (num_written < page->data_len) {
				    send_cur_offset = num_written;
				    num_written = 0;
				    msg_sent_completely = false;
				    break;
				}

				num_written -= page->data_len;
			    }

			    popPage (msg_pages);
			}
		    } else { // if (page->data_len > 0)
		      // Empty page.

			if (react)
			    popPage (msg_pages);
		    }

		    first_page = false;
		    page = next_page;
		} // while (page)
	    } break;
	    default:
		unreachable ();
	} // switch (msg_entry->type)

	if (count_iovs) {
	    if (*ret_num_iovs >= IOV_MAX)
		break;
	} else {
	    if (cur_num_iovs > num_iovs)
		break;
	}

	if (react) {
	    if (msg_sent_completely) {
	      // This is the only place where messages are removed from the queue.

		msg_list.remove (msg_entry);
		Sender::deleteMessageEntry (msg_entry);
		--num_msg_entries;

		if (send_state == Sender::QueueSoftLimit ||
		    send_state == Sender::QueueHardLimit)
		{
		    if (num_msg_entries < soft_msg_limit) {
			if (overloaded)
			    setSendState (Sender::ConnectionOverloaded);
			else
			    setSendState (Sender::ConnectionReady);
		    } else
		    if (num_msg_entries < hard_msg_limit)
			setSendState (Sender::QueueSoftLimit);
		}

		logD (send, _func, "calling resetSendingState()");
		resetSendingState ();

		if (enable_processing_barrier
		    && msg_entry == processing_barrier)
		{
		    processing_barrier = NULL;

		    if (gotDataToSend())
			processing_barrier_hit = true;

		    break;
		}
	    } else {
		assert (gotDataToSend());
		if (enable_processing_barrier
		    && msg_entry == processing_barrier)
		{
		    processing_barrier_hit = true;
		}

		break;
	    }
	} else {
	    if (enable_processing_barrier
		&& msg_entry == processing_barrier)
	    {
		break;
	    }
	}

	first_entry = false;
	msg_entry = next_msg_entry;
    } // while (msg_entry)

    if (react)
	assert (num_written == 0);
}

void
ConnectionSenderImpl::queueMessage (Sender::MessageEntry * const mt_nonnull msg_entry)
{
    if (logLevelOn (hexdump, LogLevel::Debug)) {
      // Dumping message data.

	switch (msg_entry->type) {
	    case Sender::MessageEntry::Pages: {
		Sender::MessageEntry_Pages * const msg_pages = static_cast <Sender::MessageEntry_Pages*> (msg_entry);

		// Counting message length.
		Size msg_len = 0;
		{
		    msg_len += msg_pages->header_len;

		    PagePool::Page *cur_page = msg_pages->first_page;
		    while (cur_page != NULL) {
			if (cur_page == msg_pages->first_page) {
			    assert (cur_page->data_len >= msg_pages->msg_offset);
			    msg_len += cur_page->data_len - msg_pages->msg_offset;
			} else {
			    msg_len += cur_page->data_len;
			}

			cur_page = cur_page->getNextMsgPage ();
		    }
		}

		// Collecting message data into a single adrray.
		Byte * const tmp_data = new Byte [msg_len];
		{
		    Size pos = 0;

		    memcpy (tmp_data + pos, msg_pages->getHeaderData(), msg_pages->header_len);
		    pos += msg_pages->header_len;

		    PagePool::Page *cur_page = msg_pages->first_page;
		    while (cur_page != NULL) {
			if (cur_page == msg_pages->first_page) {
			    assert (cur_page->data_len >= msg_pages->msg_offset);
			    memcpy (tmp_data + pos,
				    cur_page->getData() + msg_pages->msg_offset,
				    cur_page->data_len - msg_pages->msg_offset);
			    pos += cur_page->data_len - msg_pages->msg_offset;
			} else {
			    memcpy (tmp_data + pos, cur_page->getData(), cur_page->data_len);
			    pos += cur_page->data_len;
			}

			cur_page = cur_page->getNextMsgPage ();
		    }
		}

		logLock ();
		log_unlocked_ (libMary_logGroup_hexdump.getLogLevel(), _func, "Message data:");
		hexdump (logs, ConstMemory (tmp_data, msg_len));
		logUnlock ();

		delete tmp_data;
	    } break;
	    default:
		unreachable ();
	}
    }

    ++num_msg_entries;
    if (num_msg_entries >= hard_msg_limit)
	setSendState (Sender::QueueHardLimit);
    else
    if (num_msg_entries >= soft_msg_limit)
	setSendState (Sender::QueueSoftLimit);

    msg_list.append (msg_entry);
}

ConnectionSenderImpl::ConnectionSenderImpl (bool const enable_processing_barrier)
    : conn (NULL),
      soft_msg_limit (1024),
      hard_msg_limit (4096),
      send_state (Sender::ConnectionReady),
      overloaded (false),
      num_msg_entries (0),
      enable_processing_barrier (enable_processing_barrier),
      processing_barrier (NULL),
      processing_barrier_hit (false),
      sending_message (false),
      send_header_sent (0),
      send_cur_offset (0)
{
}

ConnectionSenderImpl::~ConnectionSenderImpl ()
{
    Sender::MessageList::iter iter (msg_list);
    while (!msg_list.iter_done (iter)) {
	Sender::MessageEntry * const msg_entry = msg_list.iter_next (iter);
	Sender::deleteMessageEntry (msg_entry);
    }
}

}

