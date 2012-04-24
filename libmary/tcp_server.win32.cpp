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


#include <libmary/tcp_server.h>


namespace M {

PollGroup::Pollable const TcpServer::pollable = {
    processEvents,
    getFd,
    setFeedback
};

void
TcpServer::processEvents (Uint32   const event_flags,
                          void   * const _self)
{
}

int
TcpServer::getFd (void * const _self)
{
    return 0;
}

void
TcpServer::setFeedback (Cb<PollGroup::Feedback> const &feedback,
                        void * const _self)
{
}

mt_throws Result
TcpServer::open ()
{
    return Result::Success;
}

mt_throws TcpServer::AcceptResult
TcpServer::accept (TcpConnection * const mt_nonnull tcp_connection,
                   IpAddress     * const ret_addr)
{
    return AcceptResult::NotAccepted;
}

mt_throws Result
TcpServer::bind (IpAddress const &ip_addr)
{
    return Result::Success;
}

mt_throws Result
TcpServer::listen ()
{
    return Result::Success;
}

mt_throws Result
TcpServer::close ()
{
    return Result::Success;
}

TcpServer::~TcpServer ()
{
}

}

