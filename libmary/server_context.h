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


#ifndef __LIBMARY__SERVER_CONTEXT__H__
#define __LIBMARY__SERVER_CONTEXT__H__


#include <libmary/timers.h>
#include <libmary/poll_group.h>
#include <libmary/deferred_processor.h>
#include <libmary/deferred_connection_sender.h>


namespace M {

class ServerThreadContext
{
private:
    mt_const Timers *timers;
    mt_const PollGroup *poll_group;
    mt_const DeferredProcessor *deferred_processor;
    mt_const DeferredConnectionSenderQueue *dcs_queue;

public:
    Timers* getTimers () const
    {
	return timers;
    }

    PollGroup* getPollGroup () const
    {
	return poll_group;
    }

    DeferredProcessor* getDeferredProcessor () const
    {
	return deferred_processor;
    }

    DeferredConnectionSenderQueue* getDeferredConnectionSenderQueue () const
    {
	return dcs_queue;
    }

    mt_const void init (Timers * const timers,
			PollGroup * const poll_group,
			DeferredProcessor * const deferred_processor,
			DeferredConnectionSenderQueue * const dcs_queue)
    {
	this->timers = timers;
	this->poll_group = poll_group;
	this->deferred_processor = deferred_processor;
	this->dcs_queue = dcs_queue;
    }

    ServerThreadContext ()
	: timers (NULL),
	  poll_group (NULL),
	  deferred_processor (NULL),
	  dcs_queue (NULL)
    {
    }
};

class ServerContext
{
private:
    mt_const Timers *timers;
    mt_const PollGroup *poll_group;

public:
    virtual ServerThreadContext* selectThreadContext () = 0;

    Timers* getTimers () const
    {
	return timers;
    }

    PollGroup* getMainPollGroup ()
    {
	return poll_group;
    }

    mt_const void init (Timers    * const timers,
			PollGroup * const main_poll_group)
    {
	this->timers = timers;
	this->poll_group = main_poll_group;
    }

    ServerContext ()
	: timers (NULL)
    {
    }

    virtual ~ServerContext () {}
};

}


#endif /* __LIBMARY__SERVER_CONTEXT__H__ */

