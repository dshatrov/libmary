/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2011-2013 Dmitry Shatrov

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


#ifndef LIBMARY__POLL_GROUP__H__
#define LIBMARY__POLL_GROUP__H__


#include <libmary/types.h>

#ifdef LIBMARY_PLATFORM_WIN32
#include <winsock2.h>
#endif

#include <libmary/cb.h>
#include <libmary/exception.h>
#include <libmary/deferred_processor.h>


namespace M {

class PollGroup : public virtual CodeReferenced
{
public:
    enum EventFlags {
	Input  = 0x1,
	Output = 0x2,
	Error  = 0x4,
	Hup    = 0x8
    };

    // TODO Unused for now. This is meant to be used for multi-threaded WsaPollGroup.
    struct Events {
        void (*pollGroupFull) (void *cb_data);
        void (*pollGroupFree) (void *cb_data);
    };

    class EventSubscriptionKey
    {
    public:
        void *ptr;
        operator bool () const { return ptr; }
        EventSubscriptionKey (void * const ptr = NULL) : ptr (ptr) {}
    };

#ifndef LIBMARY_WIN32_IOCP
    struct Feedback {
	void (*requestInput)  (void *cb_data);
	void (*requestOutput) (void *cb_data);
    };
#endif

#ifdef LIBMARY_WIN32_IOCP
    struct Overlapped : public OVERLAPPED
    {
        enum OpKind
        {
            OpKind_Read,
            OpKind_Write
        };

        OpKind op_kind;
    };
#endif

    struct Pollable
    {
#ifdef LIBMARY_WIN32_IOCP
        void (*ioComplete) (Overlapped *overlapped,
                            Size        bytes_transferred,
                            void       *cb_data);
#else
	void (*processEvents) (Uint32  event_flags,
			       void   *cb_data);
#endif

#ifdef LIBMARY_PLATFORM_WIN32
	SOCKET (*getFd) (void *cb_data);
#else
	int (*getFd) (void *cb_data);
#endif

#ifndef LIBMARY_WIN32_IOCP
	void (*setFeedback) (Cb<Feedback> const &feedback,
			     void *cb_data);
#endif
    };

    class PollableKey
    {
    private:
        void *key;
    public:
        operator void* () const { return key; }
        PollableKey (void * const key) : key (key) {}
        PollableKey () : key (NULL) {}
    };

public:
    // Every successful call to addPollable() must be matched with a call
    // to removePollable().
    virtual mt_throws PollableKey addPollable (CbDesc<Pollable> const &pollable,
					       bool activate = true) = 0;

    virtual mt_throws Result activatePollable (PollableKey mt_nonnull key) = 0;

    virtual void removePollable (PollableKey mt_nonnull key) = 0;

    virtual EventSubscriptionKey eventsSubscribe (CbDesc<Events> const & /* cb */)
    {
        return EventSubscriptionKey ();
    }

    virtual void eventsUnsubscribe (EventSubscriptionKey /* sbn_key */)
    {
    }

    virtual ~PollGroup () {}
};

}


namespace M {
    // TODO Rename DefaultPollGroup to DefaultActivePollGroup
    //      and move this into active_poll_group.h

#ifdef LIBMARY_PLATFORM_WIN32
  #ifdef LIBMARY_WIN32_IOCP
    class IocpPollGroup;
    typedef IocpPollGroup DefaultPollGroup;
  #else
    class WsaPollGroup;
    typedef WsaPollGroup DefaultPollGroup;
  #endif
#else
  #if defined (LIBMARY_USE_SELECT)
    class SelectPollGroup;
    typedef SelectPollGroup DefaultPollGroup;
  #elif defined (LIBMARY_USE_POLL) || !defined (LIBMARY_ENABLE_EPOLL)
    class PollPollGroup;
    typedef PollPollGroup DefaultPollGroup;
  #else
    class EpollPollGroup;
    typedef EpollPollGroup DefaultPollGroup;
  #endif
#endif
}
#include <libmary/active_poll_group.h>


#endif /* LIBMARY__POLL_GROUP__H__ */

