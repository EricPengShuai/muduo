#pragma once

#include "Timestamp.h"
#include "noncopyable.h"

#include <functional>
#include <memory>

class EventLoop;  // 前置声明

/**
 * 理清楚 EventLoop、Channel、Poller 之间的关系，在 Reator 模型上对应 Demultiplex
 * Channel 理解为通道，封装了 sockfd 和其感兴趣的 event，
 * 如 EPOLLIN、EPOLLOUT 事件，还绑定了 poller 返回的具体事件
 */
class Channel : noncopyable {
  public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel();

    // fd 得到 poller 通知以后，处理事件的
    void handleEvent(Timestamp receiveTime);

    // 设置回调函数
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    //!NOTE: 多线程中防止当 channel 被手动 remove 掉，channel 还在执行回调操作
    void tie(const std::shared_ptr<void> &);

    int fd() const { return fd_; }
    int events() const { return events_; }
    void set_revents(int revt) { revents_ = revt; }

    // 设置 channel.fd 读事件
    void enableReading() {
        events_ |= kReadEvent;
        update();
    }
    // 消除 channel.fd 读事件
    void disableReading() {
        events_ &= ~kReadEvent;
        update();
    }
    // 设置 channel.fd 写事件
    void enableWriting() {
        events_ |= kWriteEvent;
        update();
    }
    // 消除 channel.fd 写事件
    void disableWriting() {
        events_ &= ~kWriteEvent;
        update();
    }
    // 清空 channel.fd 所有感兴趣的事件
    void disableAll() {
        events_ = kNoneEvent;
        update();
    }

    // 返回 fd 当前的事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }
  
    // 返回 channel 状态
    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    // one loop per thread
    EventLoop *ownerLoop() { return loop_; }
    void remove();

  private:
    void update();
    void handleEventWithGuard(Timestamp receiveTime);

  private:
    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_;  // 事件循环

    const int fd_;  // fd，Poller 监听的对象
    int events_;    // 注册 fd 感兴趣的事件
    int revents_;   // poller 返回的具体发生的事件

    int index_;  // channel 状态，被 Poller 调用

    std::weak_ptr<void> tie_;
    bool tied_;

    // 因为 channel 通道里面可以获知 fd 最终发生的具体事件 revents，所以它负责调用具体事件的回调操作
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};
