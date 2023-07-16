#include "Channel.h"

#include "EventLoop.h"
#include "Logger.h"

#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI; // EPOLLPRI receive these urgent data
const int Channel::kWriteEvent = EPOLLOUT;

// 指定 loop 和 fd 初始化 channel
Channel::Channel(EventLoop *loop, int fd) 
    : loop_(loop)
    , fd_(fd)
    , events_(0)
    , revents_(0)
    , index_(-1)
    , tied_(false) {}

Channel::~Channel() {
    // if (loop_->isInLoopThread()) {
    //     assert(!loop_->hasChannel(this));
    // }
}

//!NOTE: 一个 TcpConnecetion 新连接创建的时候就会调用，强智能指针 TcpConnecetionPtr --> 弱智能指针
void Channel::tie(const std::shared_ptr<void> &obj) {
    tie_ = obj;
    tied_ = true;
}

/**
 * 当改变 channel 所表示 fd 的 events 事件之后，update 负责在 poller 里面更改 fd 相应的事件 epoll_ctl
 * EventLoop ==> ChannelList + Poller
 */
void Channel::update() {
    // 通过 channel 所属的 EventLoop，调用 poller 的相应方法，注册 fd 的 events 事件
    loop_->updateChannel(this);
}

// 在 channel 所属的 EventLoop 中把当前的 channel 删除掉
void Channel::remove() { loop_->removeChannel(this); }

// fd 得到 poller 通知之后处理事件
void Channel::handleEvent(Timestamp receiveTime) {
    if (tied_) {
        //!NOTE: 这里将 weak_ptr 提升为 shared_ptr 是为了防止 TcpConnection 被释放
        std::shared_ptr<void> guard = tie_.lock();
        if (guard) {
            handleEventWithGuard(receiveTime);
        }
    } else {
        handleEventWithGuard(receiveTime);
    }
}

// 根据 poller 通知的 channel 发生的具体事件，由 channel 负责调用具体的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime) {
    LOG_INFO("Channel::handleEventWithGuard - channel handleEvent revents: %d", revents_);

    // 异常
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
        if (closeCallback_) {
            closeCallback_();
        }
    }

    // 错误
    if (revents_ & EPOLLERR) {
        if (errorCallback_) {
            errorCallback_();
        }
    }

    // 读事件
    if (revents_ & (EPOLLIN | EPOLLPRI)) {
        if (readCallback_) {
            readCallback_(receiveTime);
        }
    }

    // 写事件
    if (revents_ & EPOLLOUT) {
        if (writeCallback_) {
            writeCallback_();
        }
    }
}
