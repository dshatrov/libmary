/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2011, 2012 Dmitry Shatrov

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


#ifndef __LIBMARY__LIBMARY__H__
#define __LIBMARY__LIBMARY__H__


#include <libmary/libmary_config.h>

#include <libmary/annotations.h>
#include <libmary/types_base.h>
#include <libmary/types.h>
#include <libmary/string.h>
#include <libmary/exception.h>
#include <libmary/informer.h>

#include <libmary/extractor.h>
#include <libmary/comparator.h>
#include <libmary/iterator.h>

#include <libmary/array.h>
#include <libmary/array_holder.h>
#include <libmary/list.h>
#include <libmary/intrusive_list.h>
#include <libmary/avl_tree.h>
#include <libmary/intrusive_avl_tree.h>
#include <libmary/map.h>
#include <libmary/hash.h>
#include <libmary/string_hash.h>
#include <libmary/page_pool.h>
#include <libmary/vstack.h>
#include <libmary/vslab.h>

#include <libmary/atomic.h>
#include <libmary/mutex.h>
#include <libmary/state_mutex.h>
#ifdef LIBMARY_MT_SAFE
#include <libmary/cond.h>
#include <libmary/thread.h>
#include <libmary/multi_thread.h>
#endif

#include <libmary/basic_referenced.h>
#include <libmary/virt_referenced.h>
#include <libmary/referenced.h>
#include <libmary/object.h>
#include <libmary/code_referenced.h>

#include <libmary/ref.h>
#include <libmary/weak_ref.h>
#include <libmary/code_ref.h>
#include <libmary/dep_ref.h>

#include <libmary/timers.h>

#include <libmary/io.h>
#include <libmary/log.h>
#include <libmary/file.h>
#include <libmary/async_file.h>
#include <libmary/native_file.h>
#include <libmary/native_async_file.h>
#include <libmary/output_stream.h>
#include <libmary/buffered_output_stream.h>
#include <libmary/array_output_stream.h>

#include <libmary/async_input_stream.h>
#include <libmary/async_output_stream.h>
#include <libmary/connection.h>
#include <libmary/file_connection.h>
#include <libmary/tcp_connection.h>
#include <libmary/tcp_server.h>
#include <libmary/sender.h>
#include <libmary/immediate_connection_sender.h>
#include <libmary/deferred_connection_sender.h>
#include <libmary/receiver.h>
#include <libmary/connection_receiver.h>

#include <libmary/line_server.h>
#include <libmary/line_service.h>
#include <libmary/line_pipe.h>

#include <libmary/vfs.h>
#ifndef LIBMARY_PLATFORM_WIN32
#include <libmary/vfs_posix.h>
#endif

#include <libmary/deferred_processor.h>
#include <libmary/poll_group.h>
#include <libmary/active_poll_group.h>
#ifdef LIBMARY_PLATFORM_WIN32
#include <libmary/wsa_poll_group.h>
#else
#include <libmary/select_poll_group.h>
#include <libmary/poll_poll_group.h>
#endif
#if !defined (LIBMARY_PLATFORM_WIN32) && defined LIBMARY_ENABLE_EPOLL
#include <libmary/epoll_poll_group.h>
#endif

#include <libmary/http.h>
#include <libmary/http_client.h>
#include <libmary/http_service.h>

#include <libmary/module.h>

#include <libmary/util_common.h>
#include <libmary/util_base.h>
#include <libmary/util_str.h>
#include <libmary/util_time.h>
#include <libmary/util_net.h>
#include <libmary/util_dev.h>
#include <libmary/libmary_md5.h>

#include <libmary/server_context.h>
#include <libmary/server_thread_pool.h>
#include <libmary/fixed_thread_pool.h>
#include <libmary/server_app.h>

#include <libmary/stat.h>


namespace M {

void libMaryInit ();

}


#endif /* __LIBMARY__LIBMARY__H__ */

