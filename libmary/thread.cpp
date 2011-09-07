#include <libmary/log.h>

#include <libmary/thread.h>


namespace M {

gpointer
Thread::wrapperThreadFunc (gpointer const _self)
{
    Thread * const self = static_cast <Thread*> (_self);

    try {
	self->mutex.lock ();
	Cb<ThreadFunc> const tmp_cb = self->thread_cb;
	self->mutex.unlock ();

	tmp_cb.call_ ();
    } catch (...) {
	logE_ (_func, "unhandled C++ exception");
    }

    self->unref ();

    return (gpointer) 0;
}

mt_throws Result
Thread::spawn (bool const joinable)
{
    this->ref ();

    mutex.lock ();
    GError *error = NULL;
    GThread * const tmp_thread = g_thread_create (wrapperThreadFunc,
						  this,
						  joinable ? TRUE : FALSE,
						  &error);
    this->thread = tmp_thread;
    mutex.unlock ();

    if (tmp_thread == NULL) {
	exc_throw <InternalException> (InternalException::BackendError);
	logE_ (_func, "g_thread_create() failed: ",
	       error->message, error->message ? strlen (error->message) : 0);
	g_clear_error (&error);

	this->unref ();

	return Result::Failure;
    }

    return Result::Success;
}

// TODO Never fails?
mt_throws Result
Thread::join ()
{
    mutex.lock ();
    assert (thread);
    GThread * const tmp_thread = thread;
    thread = NULL;
    mutex.unlock ();

    g_thread_join (tmp_thread);

    mutex.lock ();
    thread_cb.reset ();
    mutex.unlock ();

    return Result::Success;
}

void Thread::setThreadFunc (CbDesc<ThreadFunc> const &cb)
{
  StateMutexLock l (&mutex);
    this->thread_cb = cb;
}

}

