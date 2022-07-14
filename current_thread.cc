#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "current_thread.h"

namespace CurrentThread
{
    __thread int t_cachedTid = 0;

    void cacheTid()
    {
        if (0 == t_cachedTid)
        {
            // 通过 linux 系统调用获取当前线程的id,
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }
}
