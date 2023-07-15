#pragma once

#include "Timestamp.h"
#include "noncopyable.h"

#include <unordered_map>
#include <vector>

class Channel;
class EventLoop;

// moduo 库中多路事件分发器的核心 IO 复用模块
class Poller : noncopyable {
  public:
    using ChannelList = std::vector<Channel *>;

    Poller(EventLoop *loop);
    virtual ~Poller();

    // 给所有的 IO 复用保留统一的接口
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;

    // 判断参数 channel 是否在当前的 Poller 当中
    bool hasChannel(Channel *channel) const;

    // EventLoop 可以通过该接口获取默认的 IO 复用的具体实现
    //!NOTE: 这里最好不要在 Poller.cpp 中实现，因为这个需要 include EPollPoller，基类包含派生类头文件不太好
    static Poller *newDefaultPoller(EventLoop *loop);

  protected:
    //!NOTE: int 是 sockfd，Channel* 是 sockfd 所属的通道
    using ChannelMap = std::unordered_map<int, Channel *>;
    ChannelMap channels_;

  private:
    EventLoop *ownerLoop_;  // 定义 Poller 所属的事件循环 EventLoop
};
