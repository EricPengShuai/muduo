#pragma once

#include "Poller.h"
#include "Timestamp.h"

#include <sys/epoll.h>
#include <vector>

/**
 * epoll 的使用: epoll_create, epoll_ctl, epoll_wait
 * EPollPoller 就是封装了 epoll
 */
class EPollPoller : public Poller {
  public:
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override;

    // 重写基类 Poller 的抽象方法
    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;

  private:
    static const int kInitEventListSize = 16;

    // 填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;

    // 更新 channel 通道
    void update(int operation, Channel *channel);

    using EventList = std::vector<epoll_event>;

    int epollfd_;
    EventList events_;
};
