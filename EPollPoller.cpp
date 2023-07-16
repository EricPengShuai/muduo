#include "EPollPoller.h"

#include "Channel.h"
#include "Logger.h"

#include <errno.h>
#include <strings.h>
#include <unistd.h>

const int kNew = -1;     // channel 未添加到 poller 中，channel 的成员 index = -1
const int kAdded = 1;    // channel 已添加到 poller 中
const int kDeleted = 2;  // channel 被 poller 删除

EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop)
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC))  //!NOTE: EPOLL_CLOEXEC 表示子进程不会继承父进程的 fd
    , events_(kInitEventListSize)               // vector<epoll_event>
{
    if (epollfd_ < 0) {
        LOG_FATAL("EPollPoller::EPollPoller - epoll_create1 error: %d", errno);
    }
}

EPollPoller::~EPollPoller() { ::close(epollfd_); }

// 实际就是 epoll_wait 等待感兴趣的事件，并且通过 fillActiveChannels 告知 EventLoop 活跃的 channels
Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels) {
    //!TODO: poll 调用时非常频繁的，使用 LOG_DEBUG 更合适
    LOG_DEBUG("EPollPoller::poll - fd total count: %lu", activeChannels->size());

    int numEvents = ::epoll_wait(epollfd_, &(*events_.begin()), static_cast<int>(events_.size()), timeoutMs);
    int savedErrno = errno;  // 防止多线程改变 errno
    Timestamp now(Timestamp::now());

    if (numEvents > 0) {  // 监听到事件
        LOG_INFO("EPollPoller::poll - %d events happened", numEvents);
        fillActiveChannels(numEvents, activeChannels);

        if (numEvents == events_.size()) {  // vector EventList 所有，需要扩容
            events_.resize(events_.size() * 2);
        }
    } else if (numEvents == 0) {  // 超时
        LOG_DEBUG("EPollPoller::poll - %dms timeout!", timeoutMs);
    } else {  // 错误
        if (savedErrno != EINTR) { // 外部中断还需要继续处理
            errno = savedErrno;
            LOG_ERROR("EPollPoller::poll err! errno=%d", errno);
        }
    }
    return now;
}

// activeChannels->push_back 以便让 EventLoop 获取 channel 列表
void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const {
    for (int i = 0; i < numEvents; ++i) {
        Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);

        // EventLoop 就拿到了它的 Poller 给他返回的所有发生事件的 channel 列表了
        activeChannels->push_back(channel);
    }
}

/**
 * 调用关系
 * [Channel] update/remove -> [EventLoop] updateChannel/removeChannel -> [EPollPoller] updateChannel/removeChannel
 *
 *           EventLoop
 *  ChannelList     Poller
 *                  ChannelMap <fd, channel*>
 */
void EPollPoller::updateChannel(Channel *channel) {
    const int index = channel->index();
    LOG_INFO("EPollPoller::updateChannel - fd = %d, events = %d, index = %d", channel->fd(), channel->events(), index);

    // 理解 kNew, kAdded, kDeleted 之间的逻辑
    if (index == kNew || index == kDeleted) {
        if (index == kNew) {
            int fd = channel->fd();
            channels_[fd] = channel;
        }

        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    } else {  // channel 已经在 poller 上注册过了
        int fd = channel->fd();
        if (channel->isNoneEvent()) {  // 注册过但是不关心了需要删除
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        } else {  // 已经注册并需要修改的情况
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

// 从 Poller 中删除 channel
void EPollPoller::removeChannel(Channel *channel) {
    int fd = channel->fd();
    channels_.erase(fd);

    LOG_INFO("EPollPoller::removeChannel - fd = %d", fd);

    int index = channel->index(); // 获取 channel 的状态
    if (index == kAdded) {
        update(EPOLL_CTL_DEL, channel);
    }

    channel->set_index(kNew);
}

// 更新 channel 通道 epoll_ctl add/mod/del
void EPollPoller::update(int operation, Channel *channel) {
    int fd = channel->fd();

    epoll_event event;
    bzero(&event, sizeof(event));

    event.events = channel->events();
    event.data.fd = fd;
    event.data.ptr = channel;  // event.data 是联合体，注意这里 ptr 是 void* 类型，之间通常使用的是 fd

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
        if (operation == EPOLL_CTL_DEL) {
            LOG_ERROR("EPollPoller::update - epoll_ctl del error: %d", errno);
        } else {
            LOG_FATAL("EPollPoller::update - epoll_ctl add/mod error: %d", errno); // add/mod 失败是不能接受的
        }
    }
}