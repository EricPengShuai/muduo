#pragma once

#include <sys/syscall.h>
#include <unistd.h>

// 获取当前线程 id
namespace CurrentThread {

// C++11 thread_local
//!NOTE: __thread是GCC内置的线程局部存储设施。_thread变量每一个线程有一份独立实体，各个线程的值互不干扰。
// 可以用来修饰那些带有全局性且值可能变，但是又不值得用全局变量保护的变量
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