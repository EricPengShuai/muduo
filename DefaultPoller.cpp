#include "EPollPoller.h"
#include "Poller.h"

#include <stdlib.h>

Poller *Poller::newDefaultPoller(EventLoop *loop) {
    if (::getenv("MUDUO_USE_POLL")) {
        return nullptr;  // 生成 poller 实例
    } else {
        return new EPollPoller(loop);  // 生成 epoller 实例
    }
}