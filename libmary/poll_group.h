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


#ifndef __LIBMARY__POLL_GROUP__H__
#define __LIBMARY__POLL_GROUP__H__


#include <libmary/libmary_config.h>
#include <libmary/types.h>

#ifdef PLATFORM_WIN32
#include <winsock2.h>
#endif

#include <libmary/cb.h>
#include <libmary/exception.h>
#include <libmary/deferred_processor.h>


namespace M {

class PollGroup
{
public:
    enum EventFlags {
	Input  = 0x1,
	Output = 0x2,
	Error  = 0x4,
	Hup    = 0x8
    };

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

    struct Feedback {
	void (*requestInput)  (void *cb_data);
	void (*requestOutput) (void *cb_data);
    };

    struct Pollable
    {
	void (*processEvents) (Uint32  event_flags,
			       void   *cb_data);

#ifdef PLATFORM_WIN32
	SOCKET (*getFd) (void *cb_data);
#else
	int (*getFd) (void *cb_data);
#endif

	void (*setFeedback) (Cb<Feedback> const &feedback,
			     void *cb_data);
    };

    typedef void *PollableKey;

public:
    // Every successful call to addPollable() must be matched with a call
    // to removePollable().
    virtual mt_throws PollableKey addPollable (CbDesc<Pollable> const &pollable,
					       DeferredProcessor::Registration *ret_reg,
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

#ifdef PLATFORM_WIN32
    class WsaPollGroup;
    typedef WsaPollGroup DefaultPollGroup;
#else
  #if defined (LIBMARY_USE_POLL)
    class PollPollGroup;
    typedef PollPollGroup DefaultPollGroup;
    // TODO FIXME LIBMARY_PLATFORM_DEFAULT should be defined in libmary_config.h
    //            It makes no sense to use a Makefile.am-defined macros in a public header.
  #elif defined (LIBMARY_USE_SELECT) || !defined (PLATFORM_DEFAULT)
    class SelectPollGroup;
    typedef SelectPollGroup DefaultPollGroup;
  #else
    class EpollPollGroup;
    typedef EpollPollGroup DefaultPollGroup;
  #endif
#endif
}
#include <libmary/active_poll_group.h>


#endif /* __LIBMARY__POLL_GROUP__H__ */

