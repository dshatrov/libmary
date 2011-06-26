#ifndef __LIBMARY__UTIL_POSIX__H__
#define __LIBMARY__UTIL_POSIX__H__


#include <libmary/types.h>


namespace M {

mt_throws Result posix_createNonblockingPipe (int (*fd) [2]);

}


#endif /* __LIBMARY__UTIL_POSIX__H__ */

