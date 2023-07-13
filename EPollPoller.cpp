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
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC))  //!TODO: 理解一下 EPOLL_CLOEXEC
    , events_(kInitEventListSize)               // vector<epoll_event>
{
    if (epollfd_ < 0) {
        LOG_FATAL("epoll_create error: %d \n", errno);
    }
}

EPollPoller::~EPollPoller() { ::close(epollfd_); }

Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels) {
    // poll 调用时非常频繁的，实际上使用 LOG_DEBUG 更合适
    LOG_INFO("func=%s => fd total count: %lu \n", __FUNCTION__, activeChannels->size());

    int numEvents = ::epoll_wait(epollfd_, &(*events_.begin()), static_cast<int>(events_.size()), timeoutMs);
    int savedErrno = errno;  // 防止多线程改变 errno
    Timestamp now(Timestamp::now());

    if (numEvents > 0) {  // 监听到事件
        LOG_INFO("%d events happened \n", numEvents);
        fillActiveChannels(numEvents, activeChannels);

        if (numEvents == events_.size()) {  // vector EventList 所有，需要扩容
            events_.resize(events_.size() * 2);
        }
    } else if (numEvents == 0) {  // 超时
        LOG_DEBUG("%s timeout! \n", __FUNCTION__);
    } else {  // 错误
        if (savedErrno != EINTR) {
            errno = savedErrno;
            LOG_ERROR("EPollPoller::poll() err!");
        }
    }
    return now;
}

/**
 * [Channel] update/remove --> [EventLoop] updateChannel/removeChannel
 *      --> [EPollPoller] updateChannel/removeChannel
 *
 *          EventLoop
 *  ChannelList     Poller
 *                  ChannelMap <fd, channel*>
 */
void EPollPoller::updateChannel(Channel *channel) {
    const int index = channel->index();
    LOG_INFO("func=%s => fd=%d events=%d index=%d \n", __FUNCTION__, channel->fd(), channel->events(), index);

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

    LOG_INFO("func=%s => fd=%d\n", __FUNCTION__, fd);

    int index = channel->index();
    if (index == kAdded) {
        update(EPOLL_CTL_DEL, channel);
    }

    channel->set_index(kNew);
}

void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const {
    for (int i = 0; i < numEvents; ++i) {
        Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);

        // EventLoop 就拿到了它的 Poller 给他返回的所有发生事件的 channel 列表了
        activeChannels->push_back(channel);
    }
}

// 更新 channel 通道 epoll_ctl add/mod/del
void EPollPoller::update(int operation, Channel *channel) {
    int fd = channel->fd();

    epoll_event event;
    bzero(&event, sizeof(event));
    event.events = channel->events();
    // event.data.fd = fd; // todo: 应该不能一起使用吧
    event.data.ptr = channel;  // 注意这里 ptr 是 void* 类型，之间通常使用的是 fd

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
        if (operation == EPOLL_CTL_DEL) {
            LOG_ERROR("epoll_ctl del error: %d \n", errno);
        } else {
            LOG_FATAL("epoll_ctl add/mod error: %d \n", errno);
        }
    }
}