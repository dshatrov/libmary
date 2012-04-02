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


#ifndef __LIBMARY__EXCEPTION__H__
#define __LIBMARY__EXCEPTION__H__

#include <libmary/types.h>
#include <cstdlib>
#include <new>

#include <libmary/libmary_config.h>
#include <libmary/string.h>
#include <libmary/ref.h>
#include <libmary/libmary_thread_local.h>
#include <libmary/util_base.h>
#include <libmary/util_str_base.h>


namespace M {

class Exception
{
#if 0
protected:
    // Exception destructors are never called.
    // NOT TRUE when creating exceptions on stack
    ~Exception ();
#endif

public:
    Exception *cause;

    virtual Ref<String> toString ()
    {
	return grab (new String);
    }

    // TOOD toHumanString() ?

    Exception ()
	: cause (NULL)
    {
    }

    virtual ~Exception () {}
};

// TODO Use __FILE__, __LINE__ and __func__ with exceptions.

#if defined LIBMARY_MT_SAFE && !defined LIBMARY_TLOCAL // thread-local

class ExcWrapper
{
public:
    operator Exception* () const
    {
	return libMary_getThreadLocal()->exc;
    }
};

extern ExcWrapper exc;

static inline void exc_none ()
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal ();
    if (tlocal->exc_block == 0)	 {
	tlocal->exc_buffer.reset ();
	tlocal->exc = NULL;
    }
}

// TODO rename to _libMary_exc_push_mem
static inline Byte* exc_push_mem (Size const len)
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal ();
    if (tlocal->exc_block == 0) {
	Byte * const data = libMary_getThreadLocal()->exc_buffer.push (len);
	tlocal->exc = reinterpret_cast <Exception*> (data);
	return data;
    }
    return NULL;
}

// TODO rename to _libMary_exc_throw_mem
static inline Byte* exc_throw_mem (Size const len)
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal ();
    if (tlocal->exc_block == 0) {
	Byte * const data = tlocal->exc_buffer.throw_ (len);
	tlocal->exc = reinterpret_cast <Exception*> (data);
	return data;
    }
    return NULL;
}

template <class T, class ...Args>
void exc_push (Args const & ...args)
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    if (tlocal->exc_block == 0) {
	Exception * const cause = tlocal->exc;

	Byte * const data = libMary_getThreadLocal()->exc_buffer.push (sizeof (T));
	tlocal->exc = reinterpret_cast <Exception*> (data);

	new (data) T (args...);
	tlocal->exc->cause = cause;
    }
}

template <class T, class ...Args>
void exc_throw (Args const & ...args)
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    if (tlocal->exc_block == 0) {
	Byte * const data = tlocal->exc_buffer.throw_ (sizeof (T));
	tlocal->exc = reinterpret_cast <Exception*> (data);

	new (data) T (args...);
	tlocal->exc->cause = NULL;
    }
}

static inline void exc_block ()
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    ++tlocal->exc_block;
}

static inline void exc_unblock ()
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    assert (tlocal->exc_block > 0);
    --tlocal->exc_block;
}

#else // thread-local

#ifdef LIBMARY_TLOCAL
// ExceptionBuffer is non-POD, hence we only can store a pointer to it
// in thread-local storage.
extern LIBMARY_TLOCAL_SPEC ExceptionBuffer *_libMary_exc_buf;

static inline ExceptionBuffer* _libMary_get_exc_buf ()
{
  // TODO A better approach might be to require a call to libMaryInitThread()
  // for each thread.

    if (_libMary_exc_buf)
	return _libMary_exc_buf;

    _libMary_exc_buf = new ExceptionBuffer (1024 /* alloc_len */);
    return _libMary_exc_buf;
}
#else
extern ExceptionBuffer *_libMary_exc_buf;
#endif

extern LIBMARY_TLOCAL_SPEC Exception *exc;
// Block counter.
extern LIBMARY_TLOCAL_SPEC Uint32 _libMary_exc_block;

static inline void exc_none ()
{
    if (_libMary_exc_block == 0) {
#ifdef LIBMARY_TLOCAL
	_libMary_get_exc_buf()->reset ();
#else
	_libMary_exc_buf.reset ();
#endif
	exc = NULL;
    }
}

static inline Byte* exc_push_mem (Size const len)
{
    if (_libMary_exc_block == 0) {
#ifdef LIBMARY_TLOCAL
	Byte * const data = _libMary_get_exc_buf()->push (len);
#else
	Byte * const data = _libMary_exc_buf.push (len);
#endif
	exc = reinterpret_cast <Exception*> (data);
	return data;
    }
    return NULL;
}

static inline Byte* exc_throw_mem (Size const len)
{
    if (_libMary_exc_block == 0) {
#ifdef LIBMARY_TLOCAL
	Byte * const data = _libMary_get_exc_buf()->throw_ (len);
#else
	Byte * const data = _libMary_exc_buf.throw_ (len);
#endif
	exc = reinterpret_cast <Exception*> (data);
	return data;
    }
    return NULL;
}

template <class T, class ...Args>
void exc_push (Args const & ...args)
{
    if (_libMary_exc_block == 0) {
	Exception * const cause = exc;
	new (exc_push_mem (sizeof (T))) T (args...);
	exc->cause = cause;
    }
}

template <class T, class ...Args>
void exc_throw (Args const & ...args)
{
    if (_libMary_exc_block == 0) {
	new (exc_throw_mem (sizeof (T))) T (args...);
	exc->cause = NULL;
    }
}

static inline void exc_block ()
{
    ++_libMary_exc_block;
}

static inline void exc_unblock ()
{
    assert (_libMary_exc_block > 0);
    --_libMary_exc_block;
}

#endif // thread-local

#if 0 // Deprecated macros
#ifdef LIBMARY_MT_SAFE
#define exc_push(a)								\
	do {									\
	    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();	\
	    if (tlocal->exc_block == 0) {					\
		Exception * const cause = tlocal->exc;				\
		Byte * const data = libMary_getThreadLocal()->exc_buffer.push (sizeof (a));	\
		tlocal->exc = reinterpret_cast <Exception*> (data);		\
		new (data) a;							\
		tlocal->exc->cause = cause;					\
	    }									\
	} while (0)
#else
#define exc_push(a)				\
	do {					\
	    Exception * const cause = M::exc;	\
	    new (exc_push_mem (sizeof (a))) a;	\
	    M::exc->cause = cause;		\
	} while (0)
#endif

#ifdef LIBMARY_MT_SAFE
#define exc_throw(a)								\
	do {									\
	    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();	\
	    if (tlocal->exc_block == 0) {					\
		Byte * const data = tlocal->exc_buffer.throw_ (len);		\
		tlocal->exc = reinterpret_cast <Exception*> (data);		\
		new (data) a;							\
		tlocal->exc->cause = NULL;					\
	    }									\
	} while (0)
#else
#define exc_throw(a)				\
	do {					\
	    new (exc_throw_mem (sizeof (a))) a;	\
	    M::exc->cause = NULL;		\
	} while (0)
#endif

#ifdef LIBMARY_MT_SAFE
#define exc_block(tlocal)	\
	(++tlocal->exc_block)
#define exc_unblock(tlocal)			\
	do {					\
	    assert (tlocal->exc_block > 0);	\
	    --tlocal->exc_block;		\
	} while (0)
#else
#define exc_block()	\
	(++_libMary_exc_block)
#define exc_unblock()				\
	do {					\
	    assert (_libMary_exc_block > 0);	\
	    --_libMary_exc_block;		\
	} while (0)
#endif
#endif // Deprecated macros

class InternalException : public Exception
{
public:
    enum Error {
	UnknownError,
	IncorrectUsage,
	BadInput,
	FrontendError,
	BackendError,
	BackendMalfunction,
	ProtocolError,
	NotImplemented
    };

private:
    Error error;

public:
    Ref<String> toString ()
    {
	if (cause)
	    return catenateStrings ("InternalException: ", cause->toString ()->mem ());
	else
	    return grab (new String ("InternalException"));
    }

    InternalException (Error const error)
	: error (error)
    {
    }
};

class IoException : public Exception
{
public:
    // TODO toString() accepting PrintTask_List
    Ref<String> toString ()
    {
	if (cause)
	    return catenateStrings ("IoException: ", cause->toString ()->mem ());
	else
	    return grab (new String ("IoException"));
    }

#if 0
public:
    enum Error {
    };

private:
    Error error;

public:
    IoException (Error const error)
	: error (error)
    {
    }
#endif
};

#if 0
// Unused
class BadAddressException : public Exception
{
public:
    Ref<String> toString ()
    {
	if (cause)
	    return catenateStrings ("BadAddressException: ", cause->toString ()->mem ());
	else
	    return grab (new String ("BadAddressException"));
    }
};
#endif

#ifndef PLATFORM_WIN32
class PosixException : public IoException // TODO There's no reason to inherit from IoException now.
{
public:
    int errnum;

    Ref<String> toString ()
    {
	return errnoToString (errnum);
    }

    PosixException (int const errnum)
	: errnum (errnum)
    {
    }
};
#endif

class NumericConversionException : public Exception
{
public:
    enum Kind {
	EmptyString,
	NonNumericChars,
	Overflow
    };

private:
    Kind kind;

public:
    Ref<String> toString ()
    {
	// TODO Catenate with exc->toString()
	//      What's a more effective method to catenate?

	switch (kind) {
	    case EmptyString:
		return grab (new String ("Empty string"));
	    case NonNumericChars:
		return grab (new String ("String contains non-numeric characters"));
	    case Overflow:
		return grab (new String ("Value overflow"));
	}

	unreachable ();
	return NULL;
    }

    NumericConversionException (Kind const kind)
	: kind (kind)
    {
    }
};

}


#endif /* __LIBMARY__EXCEPTION__H__ */

