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


#include <libmary/deferred_connection_sender.h>
#include <libmary/util_time.h>
#include <libmary/log.h>

#include <libmary/server_app.h>


namespace M {

namespace {
LogGroup libMary_logGroup_server_app ("server_app", LogLevel::N);
}

ActivePollGroup::Frontend ServerApp::poll_frontend = {
    pollIterationBegin,
    pollIterationEnd
};

void
ServerApp::doTimerIteration ()
{
    if (!updateTime ())
	logE_ (_func, exc->toString());

    timers.processTimers ();
}

void
ServerApp::firstTimerAdded (void * const _self)
{
    logD (server_app, _func_);
    ServerApp * const self = static_cast <ServerApp*> (_self);
    self->select_poll_group.trigger ();
}

void
ServerApp::pollIterationBegin (void * const _self)
{
    ServerApp * const self = static_cast <ServerApp*> (_self);
    self->doTimerIteration ();
}

bool
ServerApp::pollIterationEnd (void * const /* _self */)
{
//    logD_ (_func_);
    return DeferredConnectionSender::pollIterationEnd ();
}

mt_throws Result
ServerApp::init ()
{
    if (!select_poll_group.open ())
	return Result::Failure;

    select_poll_group.setFrontend (Cb<ActivePollGroup::Frontend> (&poll_frontend, this, getCoderefContainer()));

    return Result::Success;
}

mt_throws Result
ServerApp::run ()
{
    for (;;) {
	if (!select_poll_group.poll (timers.getSleepTime_microseconds()))
	    return Result::Failure;

// Deprecated.	doTimerIteration ();
    }

    return Result::Success;
}

}

