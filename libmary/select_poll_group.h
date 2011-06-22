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


#ifndef __LIBMARY__SELECT_POLL_GROUP__H__
#define __LIBMARY__SELECT_POLL_GROUP__H__


#include <libmary/types.h>
#include <libmary/libmary_thread_local.h>
#include <libmary/cb.h>
#include <libmary/basic_referenced.h>
#include <libmary/intrusive_list.h>
#include <libmary/active_poll_group.h>
#include <libmary/code_referenced.h>


namespace M {

class SelectPollGroup : public ActivePollGroup,
			public DependentCodeReferenced
{
public:
    typedef void (*IterationBeginCallback) (void *cb_data);
    typedef void (*IterationEndCallback)   (void *cb_data);

    struct Frontend {
	// pollIterationBegin is not called when poll() returns (poll timeout/error).
	void (*pollIterationBegin) (void *cb_data);
	void (*pollIterationEnd)   (void *cb_data);
    };

private:
    class PollableList_name;
    class SelectedList_name;

    class PollableEntry : public BasicReferenced,
			  public IntrusiveListElement<PollableList_name>,
			  public IntrusiveListElement<SelectedList_name>
    {
    public:
	mt_const SelectPollGroup *select_poll_group;

	bool valid;

	Cb<Pollable> pollable;

	int fd;

	bool need_input;
	bool need_output;
    };

    typedef IntrusiveList <PollableEntry, PollableList_name> PollableList;
    typedef IntrusiveList <PollableEntry, SelectedList_name> SelectedList;

    PollableList pollable_list;

    mt_const int trigger_pipe [2];
    bool triggered;

    mt_const Cb<Frontend> frontend;

    StateMutex mutex;

    // Accessed from the same thread only.
    LibMary_ThreadLocal *poll_tlocal;

    mt_throws Result triggerWrite ();

    static Feedback const pollable_feedback;

    static void requestInput (void *_pollable_entry);

    static void requestOutput (void *_pollable_entry);

public:
  mt_iface (ActivePollGroup)

    mt_iface (PollGroup)

      mt_throws PollableKey addPollable (Cb<Pollable> const &pollable);

      mt_throws Result removePollable (PollableKey mt_nonnull key);

    mt_iface_end (PollGroup)

    // Must be called from the same thread every time.
    mt_throws Result poll (Uint64 timeout_microsec = (Uint64) -1);

    mt_throws Result trigger ();

  mt_iface_end (ActivePollGroup)

    void setFrontend (Cb<Frontend> const &frontend)
    {
	this->frontend = frontend;
    }

    mt_throws Result open ();

    SelectPollGroup (Object * const coderef_container)
	: DependentCodeReferenced (coderef_container),
	  triggered (false),
	  poll_tlocal (NULL)
    {
	trigger_pipe [0] = -1;
	trigger_pipe [1] = -1;
    }

    ~SelectPollGroup ();
};

}


#endif /* __LIBMARY__SELECT_POLL_GROUP__H__ */

