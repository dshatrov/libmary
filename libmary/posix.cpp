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


#include <libmary/types.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

#include <libmary/exception.h>
#include <libmary/posix.h>
#include <libmary/io.h>
#include <libmary/native_file.h>
#include <libmary/buffered_output_stream.h>
#include <libmary/log.h>
#include <libmary/util_str.h>


namespace M {

#if 0
void posix_throw_errno ()
{
//    switch (errno) {
//    }
    exc_throw (PosixException (errno));
    exc_push ();

#if 0
    // TODO Convert posix errors to libmary.
    exc_throw (InternalException (InternalException::UnknownError));
#endif
}
#endif

void libMary_posixInit ()
{
    // Calling tzset() for localtime_r() to behave correctly.
    tzset ();

    outs = new NativeFile (STDOUT_FILENO);
    errs = new NativeFile (STDERR_FILENO);

#if 0
    static BufferedOutputStream logs_buffered (errs, 4096);
    logs = &logs_buffered;
#endif
    // Allocating on heap to avoid problems with deinitialization order
    // of static data.
    logs = new BufferedOutputStream (errs, 4096);

    if (!updateTime ())
	logE_ (_func, exc->toString());

    // Blocking SIGPIPE for write()/writev().
    {
//	logD_ (_func, "blocking SIGPIPE");

	struct sigaction act;
	memset (&act, 0, sizeof (act));
	act.sa_handler = SIG_IGN;
	// TODO -1 on error
	sigemptyset (&act.sa_mask);
	// TODO -1 on error
	sigaddset (&act.sa_mask, SIGPIPE);
	if (sigaction (SIGPIPE, &act, NULL) == -1)
	    logE_ (_func, "sigaction() failed: ", errnoString (errno));
    }
#if 0
    {
	struct sigaction sa;
	zeroMemory (&sa, sizeof sa);
	sa.sa_handler = sigchld_handler;
	sigemptyset (&sa.sa_mask);
/* I'd like to have a non-restarting signal
 * to test MyNC libraries for correct handling of EINTR.
	sa.sa_flags = SA_RESTART;
 */
	// TODO sigaddset?
	sa.sa_flags = 0;
	if (sigaction (SIGCHLD, &sa, NULL) == -1) {
	    printError ("sigthread_func: (fatal) sigaction");
	    _exit (-1);
	}
    }
#endif
}

}

