#include "CurrentThread.h"

namespace CurrentThread {
__thread int t_cachedTid = 0;

void cacheTid() {
    if (t_cachedTid == 0) {
        // 通过 Linux 系统调用获取当前线程的系统调用
        t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
    }
}
}  // namespace CurrentThread