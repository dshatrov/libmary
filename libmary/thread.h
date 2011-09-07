#ifndef __LIBMARY__THREAD__H__
#define __LIBMARY__THREAD__H__


#include <libmary/libmary_config.h>

#include <libmary/types.h>
#include <glib/gthread.h>

#include <libmary/exception.h>
#include <libmary/cb.h>


namespace M {

class Thread : public Object
{
public:
    typedef void ThreadFunc (void *cb_data);

private:
    // 'thread_cb' gets reset when the thread exits.
    mt_mutex (mutex) Cb<ThreadFunc> thread_cb;

    mt_mutex (mutex) GThread *thread;

    static gpointer wrapperThreadFunc (gpointer _self);

public:
    // Should be called only once. May be called again after join() completes.
    mt_throws Result spawn (bool joinable);

    // Should be called once after every call to spawn(true /* joinable */).
    mt_throws Result join ();

    // Should be called only once. May be called again after join() completes.
    // Thread callback is reset when the thread exits.
    void setThreadFunc (CbDesc<ThreadFunc> const &cb);

    Thread ()
	: thread (NULL)
    {
    }
};

}


#endif /* __LIBMARY__THREAD__H__ */

