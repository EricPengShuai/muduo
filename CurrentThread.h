#pragma once

#include <sys/syscall.h>
#include <unistd.h>

// 获取当前线程 id
namespace CurrentThread {

//!TODO: __thread 是 thread_local 机制
extern __thread int t_cachedTid;

void cacheTid();

inline int tid() {
    // 底层优化的一个参数
    if (__builtin_expect(t_cachedTid == 0, 0)) {
        cacheTid();
    }

    return t_cachedTid;
}
}  // namespace CurrentThread