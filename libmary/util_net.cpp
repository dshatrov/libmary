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

#include <cstring>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include <libmary/log.h>
#include <libmary/util_base.h>

#include <libmary/util_net.h>


namespace M {

Result splitHostPort (ConstMemory const &hostname,
		      ConstMemory * const ret_host,
		      ConstMemory * const ret_port)
{
    if (ret_host)
	*ret_host = ConstMemory ();
    if (ret_port)
	*ret_port = ConstMemory ();

    void const * const colon = memchr (hostname.mem(), ':', hostname.len());
    if (colon == NULL) {
	logE_ (_func, "no colon found in hostname: ", hostname);
	return Result::Failure;
    }

    if (ret_host)
	*ret_host = hostname.region (0, (Byte const*) colon - hostname.mem());

    if (ret_port)
	*ret_port = hostname.region (((Byte const *) colon - hostname.mem()) + 1);

    return Result::Success;
}

Result hostToIp (ConstMemory const &host,
		 Uint32 * const ret_addr)
{
    if (host.len() == 0) {
	if (ret_addr)
	    *ret_addr = INADDR_ANY;

	return Result::Success;
    }

    if (ret_addr)
	memset (ret_addr, 0, sizeof (*ret_addr));

    char host_str [1025];
    if (host.len() >= sizeof (host_str)) {
	logE_ (_func, "host name is too long: ", sizeof (host_str) - 1, " bytes max, got ", host.len(), " bytes: ", host);
	return Result::Failure;
    }
    memcpy (host_str, host.mem(), host.len());
    host_str [host.len()] = 0;

    struct sockaddr_in addr;
#ifndef PLATFORM_WIN32
    if (!inet_aton (host_str, &addr.sin_addr))
#else
    int addr_len = sizeof (addr.sin_addr);
    if (WSAStringToAddress (host_str, AF_INET, NULL, (struct sockaddr*) addr, &addr_len))
#endif
    {
#ifdef PLATFORM_WIN32
	libraryLock ();
	struct hostent * const he_res = gethostbyname (host_str);
	if (!he_res) {
	    libraryUnlock ();
	    return Result::Failure;
	}
#else
	struct hostent he_buf;
	struct hostent *he_res = NULL;
	char he_str_buf_base [1024];
	char *he_str_buf = he_str_buf_base;
	size_t he_str_buf_size = sizeof (he_str_buf_base);
	int herr;
	for (;;) {
	    // TODO: From the manpage:
	    //     POSIX.1-2008 removes the specifications of gethostbyname(), gethostbyaddr(),
	    //     and h_errno, recommending the use of getaddrinfo(3) and getnameinfo(3) instead.
	    int const res = gethostbyname_r (host_str, &he_buf, he_str_buf, he_str_buf_size, &he_res, &herr);
	    if (res == ERANGE) {
		he_str_buf_size <<= 1;
		assert (he_str_buf_size);
		// We don't want the workbuffer to grow larger than 1Mb (a wild guess).
		if (he_str_buf_size > (1 << 20)) {
		    logE_ (_func, "gethostbyname_r(): 1 Mb workbuffer size limit hit");
		    return Result::Failure;
		}

		if (he_str_buf != he_str_buf_base)
		    delete[] he_str_buf;

		he_str_buf = new char [he_str_buf_size];
		assert (he_str_buf);
		continue;
	    }

	    if (res > 0) {
		logE_ (_func, "gethostbyname_r() failed: ", errnoString (res));

		if (he_str_buf != he_str_buf_base)
		    delete[] he_str_buf;

		return Result::Failure;
	    } else
	    if (res != 0) {
		logE_ (_func, "gethostbyname_r(): unexpected return value: ", res);

		if (he_str_buf != he_str_buf_base)
		    delete[] he_str_buf;

		return Result::Failure;
	    }

	    break;
	}
	assert (he_res);
#endif

	addr.sin_addr = *(struct in_addr*) he_res->h_addr;

#ifdef PLATFORM_WIN32
	libraryUnlock ();
#else
	if (he_str_buf != he_str_buf_base)
	    delete[] he_str_buf;
#endif
    }

    if (ret_addr)
	*ret_addr = ntohl (addr.sin_addr.s_addr);

    return Result::Success;
}

Result serviceToPort (ConstMemory const &service,
		      Uint16 * const ret_port)
{
    char service_str [1025];
    if (service.len() >= sizeof (service_str)) {
	logE_ (_func, "service name is too long: ", sizeof (service_str) - 1, " bytes max, got ", service.len(), " bytes: ", service);
	return Result::Failure;
    }
    memcpy (service_str, service.mem(), service.len());
    service_str [service.len()] = 0;

    char *endptr;
    Uint16 port = (Uint16) strtoul (service_str, &endptr, 0);
    if (*endptr != 0) {
	struct servent se_buf;
	struct servent *se_res = NULL;
	char se_str_buf_base [1024];
	char *se_str_buf = se_str_buf_base;
	size_t se_str_buf_size = sizeof (se_str_buf_base);
	for (;;) {
	    int const res = getservbyname_r (service_str, "tcp", &se_buf, se_str_buf, se_str_buf_size, &se_res);
	    if (res == ERANGE) {
		se_str_buf_size <<= 1;
		assert (se_str_buf_size);
		// We don't want the workbuffer to grow larger than 1Mb (a wild guess).
		if (se_str_buf_size > (1 << 20)) {
		    logE_ (_func, "getservbyname_r(): 1 Mb workbuffer size limit hit");
		    return Result::Failure;
		}

		if (se_str_buf != se_str_buf_base)
		    delete[] se_str_buf;

		se_str_buf = new char [se_str_buf_size];
		assert (se_str_buf);
		continue;
	    }

	    if (res > 0) {
		logE_ (_func, "getservbyname_r() failed: ", errnoString (res));

		if (se_str_buf != se_str_buf_base)
		    delete[] se_str_buf;

		return Result::Failure;
	    } else
	    if (res != 0) {
		logE_ (_func, "getservbyname_r(): unexpected return value: ", res);

		if (se_str_buf != se_str_buf_base)
		    delete[] se_str_buf;

		return Result::Failure;
	    }

	    break;
	}
	assert (se_res);

	port = se_res->s_port;

	if (se_str_buf != se_str_buf_base)
	    delete[] se_str_buf;
    }

    if (ret_port)
	*ret_port = port;

    return Result::Success;
}

void setIpAddress (Uint32 const ip_addr,
		   Uint16 const port,
		   struct sockaddr_in * const ret_addr)
{
    if (ret_addr) {
	memset (ret_addr, 0, sizeof (*ret_addr));
	ret_addr->sin_family = AF_INET;
	ret_addr->sin_addr.s_addr = htonl (ip_addr);
	ret_addr->sin_port = htons (port);
    }
}

}

