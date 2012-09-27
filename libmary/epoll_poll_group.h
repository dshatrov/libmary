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


#ifndef __LIBMARY__EPOLL_POLL_GROUP__H__
#define __LIBMARY__EPOLL_POLL_GROUP__H__


#include <libmary/types.h>
#include <libmary/libmary_thread_local.h>
#include <libmary/cb.h>
#include <libmary/intrusive_list.h>
#include <libmary/active_poll_group.h>
#include <libmary/code_referenced.h>


namespace M {

class EpollPollGroup : public ActivePollGroup,
                       public DependentCodeReferenced
{
private:
    StateMutex mutex;

    class PollableList_name;
    class PollableDeletionQueue_name;

    class PollableEntry : public IntrusiveListElement<PollableList_name>,
			  public IntrusiveListElement<PollableDeletionQueue_name>
    {
    public:
	mt_const EpollPollGroup *epoll_poll_group;

	mt_const Cb<Pollable> pollable;
	mt_const int fd;

	mt_mutex (EpollPollGroup::mutex) bool valid;
    };

    typedef IntrusiveList<PollableEntry, PollableList_name> PollableList;
    typedef IntrusiveList<PollableEntry, PollableDeletionQueue_name> PollableDeletionQueue;

    mt_const int efd;

    mt_const int trigger_pipe [2];
    mt_mutex (mutex) bool triggered;
    mt_mutex (mutex) bool block_trigger_pipe;

    // Should be accessed from event processing thread only.
    bool got_deferred_tasks;

    mt_mutex (mutex) PollableList pollable_list;
    mt_mutex (mutex) PollableDeletionQueue pollable_deletion_queue;

    DeferredProcessor deferred_processor;

    mt_const LibMary_ThreadLocal *poll_tlocal;

    mt_throws Result doActivate (PollableEntry * mt_nonnull pollable_entry);

    mt_mutex (mutex) void processPollableDeletionQueue ();

    mt_throws Result triggerPipeWrite ();

public:
  mt_iface (ActivePollgroup)
    mt_iface (PollGroup)
      mt_throws PollableKey addPollable (CbDesc<Pollable> const &pollable,
					 DeferredProcessor::Registration *ret_reg,
					 bool activate = true);

      mt_throws Result activatePollable (PollableKey mt_nonnull key);

      void removePollable (PollableKey mt_nonnull key);
    mt_end

    mt_throws Result poll (Uint64 timeout_microsec = (Uint64) -1);

    mt_throws Result trigger ();
  mt_end

    mt_throws Result open ();

    mt_const void bindToThread (LibMary_ThreadLocal * const poll_tlocal)
    {
	this->poll_tlocal = poll_tlocal;
    }

    EpollPollGroup (Object *coderef_container);

    ~EpollPollGroup ();
};

}


#endif /* __LIBMARY__EPOLL_POLL_GROUP__H__ */

