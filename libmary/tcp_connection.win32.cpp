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


#include <libmary/tcp_connection.h>


namespace M {

PollGroup::Pollable const TcpConnection::pollable = {
    processEvents,
    getFd,
    setFeedback
};

void
TcpConnection::processEvents (Uint32   const event_flags,
                              void   * const _self)
{
}

int
TcpConnection::getFd (void * const _self)
{
    return 0;
}

void
TcpConnection::setFeedback (Cb<PollGroup::Feedback> const &feedback,
                            void * const _self)
{
}

mt_throws AsyncIoResult
TcpConnection::read (Memory const &mem,
                     Size * const ret_nwritten)
{
    return AsyncIoResult::Normal;
}

mt_throws AsyncIoResult
TcpConnection::write (ConstMemory const &mem,
                      Size * const ret_nwritten)
{
    return AsyncIoResult::Normal;
}

mt_throws AsyncIoResult
TcpConnection::writev (struct iovec * const iovs,
                       Count          const num_iovs,
                       Size         * const ret_nwritten)
{
    return AsyncIoResult::Normal;
}

mt_throws Result
TcpConnection::close ()
{
    return Result::Success;
}

mt_throws Result
TcpConnection::connect (IpAddress const &ip_addr)
{
    return Result::Success;
}

TcpConnection::TcpConnection (Object * const coderef_container)
    : DependentCodeReferenced (coderef_container)
{
}

TcpConnection::~TcpConnection ()
{
}

}

