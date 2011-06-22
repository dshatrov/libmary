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


#ifndef __LIBMARY__SERVER_APP__H__
#define __LIBMARY__SERVER_APP__H__


#include <libmary/types.h>
#include <libmary/code_referenced.h>
#include <libmary/exception.h>
#include <libmary/timers.h>
#include <libmary/select_poll_group.h>


namespace M {

class ServerApp : public DependentCodeReferenced
{
private:
    Timers timers;
    SelectPollGroup select_poll_group;

    void doTimerIteration ();

    static void firstTimerAdded (void *_self);

  mt_iface (SelectPollGroup::Frontend)

    static SelectPollGroup::Frontend poll_frontend;

    static void pollIterationBegin (void *_self);

    static void pollIterationEnd (void *_self);

  mt_iface_end()

public:
    Timers* getTimers ()
    {
	return &timers;
    }

    PollGroup* getPollGroup ()
    {
	return &select_poll_group;
    }

    mt_throws Result init ();

    mt_throws Result run ();

    ServerApp (Object * const coderef_container)
	: DependentCodeReferenced (coderef_container),
	  timers (firstTimerAdded, this, coderef_container),
	  select_poll_group (coderef_container)
    {
    }
};

}


#endif /* __LIBMARY__SERVER_APP__H__ */

