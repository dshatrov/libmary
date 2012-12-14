COMMON_CFLAGS =				\
	-ggdb				\
	-Wno-long-long -Wall            \
	-include "local_config.h"	\
	$(THIS_CFLAGS)

if LIBMARY_PLATFORM_WIN32
    COMMON_CFLAGS +=
endif
if LIBMARY_PLATFORM_CYGWIN
    COMMON_CFLAGS += -pthread
endif

if LIBMARY_PLATFORM_DEFAULT
    COMMON_CFLAGS += -pthread
    # For backtraces
    COMMON_CFLAGS += -rdynamic
endif

AM_CFLAGS   += -std=gnu99 $(COMMON_CFLAGS)
AM_CXXFLAGS += $(COMMON_CFLAGS)

INCLUDES = -I$(top_srcdir) -I$(top_builddir)

mary_private_headers =

mary_target_headers =			\
	libmary_config.h		\
	debug.h				\
					\
	libmary.h			\
	util_base.h			\
	util_str_base.h			\
	util_str.h			\
	util_time.h			\
	util_net.h			\
	util_dev.h			\
	util_common.h			\
        libmary_md5.h                   \
					\
	annotations.h			\
	types_base.h			\
	types.h				\
	memory.h			\
	string.h			\
	exception_buffer.h		\
	exception.h			\
	cb.h				\
        cb_deferred.h                   \
	informer.h			\
					\
	pointer.h			\
	extractor.h			\
	comparator.h			\
	iterator.h			\
					\
	array.h				\
	array_holder.h			\
	list.h				\
	intrusive_list.h		\
	avl_tree.h			\
	intrusive_avl_tree.h		\
	map.h				\
	hash.h				\
	string_hash.h			\
	page_pool.h			\
	vstack.h			\
	vslab.h				\
					\
	atomic.h			\
	mutex.h				\
	state_mutex.h			\
					\
	basic_referenced.h		\
	virt_referenced.h		\
	code_referenced.h		\
	referenced.h			\
	object.h			\
					\
	virt_ref.h			\
	ref.h				\
	weak_ref.h			\
	code_ref.h			\
        dep_ref.h                       \
					\
	libmary_thread_local.h		\
	deletion_queue.h		\
					\
	timers.h			\
					\
	io.h				\
	log.h				\
	file.h				\
        async_file.h                    \
	native_file.h			\
        native_async_file.h             \
	input_stream.h			\
	output_stream.h			\
	buffered_output_stream.h	\
	array_output_stream.h		\
					\
	async_input_stream.h		\
	async_output_stream.h		\
	connection.h			\
	file_connection.h		\
	tcp_connection.h		\
	tcp_server.h			\
	sender.h			\
	connection_sender_impl.h	\
	immediate_connection_sender.h	\
	deferred_connection_sender.h	\
	receiver.h			\
	connection_receiver.h		\
                                        \
        line_server.h                   \
        line_service.h                  \
        line_pipe.h                     \
					\
	vfs.h				\
	vfs_posix.h			\
					\
	deferred_processor.h		\
	poll_group.h			\
	active_poll_group.h		\
	select_poll_group.h		\
					\
	http.h				\
        http_client.h                   \
	http_service.h			\
					\
	module.h			\
	module_init.h			\
					\
	server_context.h		\
	server_thread_pool.h		\
	fixed_thread_pool.h		\
	server_app.h                    \
                                        \
        stat.h

mary_mtsafe_headers = 			\
	cond.h				\
	thread.h			\
	multi_thread.h

if LIBMARY_MT_SAFE
mary_target_headers += $(mary_mtsafe_headers)
else
mary_private_headers += $(mary_mtsafe_headers)
endif

mary_linux_target_headers =	\
	posix.h			\
	util_posix.h		\
	native_file.linux.h	\
	tcp_connection.linux.h	\
	tcp_server.linux.h	\
	poll_poll_group.h

mary_win32_target_headers =	\
	win32.h			\
	native_file.win32.h	\
	tcp_connection.win32.h	\
	tcp_server.win32.h      \
        wsa_poll_group.h

if LIBMARY_ENABLE_EPOLL
    mary_linux_target_headers += epoll_poll_group.h
else
    mary_private_headers += epoll_poll_group.h
endif

if LIBMARY_ENABLE_MWRITEV
    mary_linux_target_headers += mwritev.h
else
    mary_private_headers += mwritev.h
endif

if LIBMARY_PLATFORM_WIN32
    mary_target_headers  += $(mary_win32_target_headers)
    mary_private_headers += $(mary_linux_target_headers)
else
    mary_target_headers  += $(mary_linux_target_headers)
    mary_private_headers += $(mary_win32_target_headers)
endif

MARY_GENFILES =			\
	native_file.h		\
        native_async_file.h     \
	tcp_connection.h	\
	tcp_server.h
BUILT_SOURCES = $(MARY_GENFILES)
CLEANFILES = $(MARY_GENFILES)
nodist_libmary_1_0_la_SOURCES = $(MARY_GENFILES)

DISTCLEANFILES = libmary_config.h

.PHONY: native_file.h           \
        native_async_file.h     \
        tcp_connection.h        \
        tcp_server.h

if LIBMARY_PLATFORM_WIN32
native_file.h: native_file.win32.h
	rm -f $@
	ln -s $(srcdir)/native_file.win32.h native_file.h

native_async_file.h: native_async_file.win32.h
	rm -f $@
	ln -s $(srcdir)/native_async_file.win32.h native_async_file.h

tcp_connection.h: tcp_connection.win32.h
	rm -f $@
	ln -s $(srcdir)/tcp_connection.win32.h tcp_connection.h

tcp_server.h: tcp_server.win32.h
	rm -f $@
	ln -s $(srcdir)/tcp_server.win32.h tcp_server.h
else
native_file.h: native_file.linux.h
	rm -f $@
	ln -s $(srcdir)/native_file.linux.h native_file.h

native_async_file.h: native_async_file.linux.h
	rm -f $@
	ln -s $(srcdir)/native_async_file.linux.h native_async_file.h

tcp_connection.h: tcp_connection.linux.h
	rm -f $@
	ln -s $(srcdir)/tcp_connection.linux.h tcp_connection.h

tcp_server.h: tcp_server.linux.h
	rm -f $@
	ln -s $(srcdir)/tcp_server.linux.h tcp_server.h
endif

libmaryincludedir = $(includedir)/libmary-1.0/libmary
libmaryinclude_HEADERS = $(mary_target_headers)

lib_LTLIBRARIES = libmary-1.0.la
libmary_1_0_la_SOURCES =		\
	util_c.c			\
	util_base.cpp			\
        util_common.cpp                 \
	util_str.cpp			\
	util_time.cpp			\
	util_net.cpp			\
	util_dev.cpp			\
					\
	types.cpp			\
	string.cpp			\
        exception_buffer.cpp            \
	exception.cpp			\
	informer.cpp			\
					\
	page_pool.cpp			\
					\
	state_mutex.cpp			\
					\
	referenced.cpp			\
	object.cpp			\
					\
	libmary_thread_local.cpp	\
	deletion_queue.cpp		\
					\
	timers.cpp			\
					\
	log.cpp				\
	file.cpp			\
	input_stream.cpp		\
	output_stream.cpp		\
	buffered_output_stream.cpp	\
					\
	vfs.cpp				\
	vfs_posix.cpp			\
					\
	async_output_stream.cpp		\
	file_connection.cpp		\
	sender.cpp			\
	connection_sender_impl.cpp	\
	immediate_connection_sender.cpp	\
	deferred_connection_sender.cpp	\
	receiver.cpp			\
	connection_receiver.cpp		\
                                        \
        line_server.cpp                 \
        line_service.cpp                \
        line_pipe.cpp                   \
					\
	deferred_processor.cpp		\
					\
	http.cpp			\
        http_client.cpp                 \
	http_service.cpp		\
					\
	module.cpp			\
					\
	fixed_thread_pool.cpp		\
	server_app.cpp                  \
                                        \
        stat.cpp                        \
                                        \
        md5/md5.c                       \
        libmary_md5.cpp                 \
                                        \
	libmary.cpp

mary_extra_dist = libmary_config.h.in

mary_mtsafe_sources =			\
	cond.cpp			\
	thread.cpp			\
	multi_thread.cpp

if LIBMARY_MT_SAFE
libmary_1_0_la_SOURCES += $(mary_mtsafe_sources)
else
mary_extra_dist += $(mary_mtsafe_sources)
endif

mary_linux_sources =			\
	posix.cpp			\
	util_posix.cpp			\
	native_file.linux.cpp		\
        native_async_file.linux.cpp     \
	tcp_connection.linux.cpp	\
	tcp_server.linux.cpp		\
	select_poll_group.cpp		\
	poll_poll_group.cpp

mary_win32_sources =			\
        win32.cpp                       \
	native_file.win32.cpp		\
        native_async_file.win32.cpp     \
	tcp_connection.win32.cpp	\
	tcp_server.win32.cpp            \
	wsa_poll_group.cpp

if LIBMARY_ENABLE_EPOLL
    mary_linux_sources += epoll_poll_group.cpp
else
    mary_extra_dist += epoll_poll_group.cpp
endif

if LIBMARY_ENABLE_MWRITEV
    mary_linux_sources += mwritev.cpp
else
    mary_extra_dist += mwritev.cpp
endif

if LIBMARY_PLATFORM_WIN32
    libmary_1_0_la_SOURCES += $(mary_win32_sources)
    mary_extra_dist += $(mary_linux_sources)
else
    libmary_1_0_la_SOURCES += $(mary_linux_sources)
    mary_extra_dist += $(mary_win32_sources)
endif

libmary_1_0_la_LDFLAGS = -no-undefined -version-info "0:0:0"
libmary_1_0_la_LIBADD = $(THIS_LIBS)
if LIBMARY_PLATFORM_WIN32
libmary_1_0_la_LIBADD += -lws2_32 -lwinmm
endif

EXTRA_DIST = $(mary_private_headers) $(mary_extra_dist)

