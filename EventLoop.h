#pragma once

#include "CurrentThread.h"
#include "Timestamp.h"
#include "noncopyable.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

class Channel;  // 前置声明
class Poller;

/* 事件循环类，主要包含两大模块 Channel + Poller（epoll 的抽象） */
class EventLoop : noncopyable {
  public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop();  // 开启事件循环
    void quit();  // 退出事件循环

    Timestamp pollReturnTime() const { return pollReturnTime_; }

    void runInLoop(Functor cb);    // 在当前 loop 中执行 cb
    void queueInLoop(Functor cb);  // 把 cb 放入队列中，唤醒 loop 所在的线程，执行 cb

    void wakeup();  // 用来唤醒 loop 所在的线程

    // EventLoop 调用 Poller 方法
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    void hasChannel(Channel *channel);

    // 判断 EventLoop 对象是否在自己的线程里面
    bool isInLoopThread() { return threadId_ == CurrentThread::tid(); }

  private:
    void handleRead();         // 处理 wakeup
    void doPendingFunctors();  // 执行回调

    using ChannelList = std::vector<Channel *>;

    std::atomic_bool looping_;  // todo: 原子操作，通过 CAS 实现
    std::atomic_bool quit_;     // 标识退出 loop 循环

    const pid_t threadId_;      // 记录当前 loop 的线程 id
    Timestamp pollReturnTime_;  // poller 返回发生事件的 channels 的时间点
    std::unique_ptr<Poller> poller_;

    //!NOTE: 理解 eventfd()
    //!NOTE: 主要作用，当 mainLoop 获取一个新用户的 channel，通过轮询算法选择一个 subloop，通过该成员唤醒subloop 处理 channel
    int wakeupFd_;

    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activateChannels_;
    // Channel *currentActivateChannels_; // assert

    std::atomic_bool callingPendingFunctors_;  // 表示当前 loop 是否有需要执行的回调操作
    std::vector<Functor> pendingFunctors_;     // 存储 loop 需要执行的所有回调操作
    std::mutex mutex_;                         // 互斥锁，用来保护上面 vector 容器的线程安全操作
};
