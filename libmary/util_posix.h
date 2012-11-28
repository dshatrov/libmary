#ifndef __LIBMARY__UTIL_POSIX__H__
#define __LIBMARY__UTIL_POSIX__H__


#include <libmary/types.h>

#include <sys/types.h>
#include <sys/stat.h>


namespace M {

mt_throws Result posix_createNonblockingPipe (int (*fd) [2]);

mt_throws Result commonTriggerPipeWrite (int fd);

mt_throws Result commonTriggerPipeRead (int fd);

mt_throws Result posix_statToFileStat (struct stat * mt_nonnull stat_buf,
                                       FileStat    * mt_nonnull ret_stat);

}


#endif /* __LIBMARY__UTIL_POSIX__H__ */

