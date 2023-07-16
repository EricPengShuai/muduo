#include "EventLoop.h"

#include "Channel.h"
#include "Logger.h"
#include "Poller.h"

#include <errno.h>
#include <sys/eventfd.h>

// 防止一个线程创建多个 EventLoop，thread_local 机制
__thread EventLoop *t_loopInThisThread = nullptr;

// 定义默认的 Poller IO 复用接口的超时时间 10s
const int kPollTimeMs = 10000;

// 创建 wakeupfd，用来 notify 唤醒 subReactor 处理新来的 channel
int createEventfd() {
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0) {
        LOG_FATAL("EventLoop createEventfd() - eventfd error: %d", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , wakeupFd_(createEventfd())
    , wakeupChannel_(new Channel(this, wakeupFd_))
// , currentActivateChannels_(nullptr)
{
    LOG_DEBUG("EventLoop::EventLoop() - created %p in thread %d", this, threadId_);
    if (t_loopInThisThread) {
        LOG_FATAL("EventLoop::EventLoop() - another EventLoop %p exists in this thread %d", t_loopInThisThread, threadId_);
    } else {
        t_loopInThisThread = this;
    }

    // 设置 wakeupfd 的事件类型以及发生事件之后的回调操作
    //!NOTE: 注意 bind 和 callback 的使用
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));

    // 每一个 eventloop 都将监听 wakeupchannel 的 EPOLLIN 读事件了
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

// 开启事件循环
void EventLoop::loop() {
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop::loop() - %p start looping", this);

    while (!quit_) {
        // 首先清空 channels
        activateChannels_.clear();

        // 监听两类 fd，一种是 client 的 fd，一种是 wakeupfd
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activateChannels_);

        for (Channel *channel : activateChannels_) {
            // poller 监听哪些 channel 发生事件了，然后上报给 EventLoop，通知 channel 处理相应的事件
            channel->handleEvent(pollReturnTime_);
        }

        // 执行当前 EventLoop 事件循环需要处理的回调操作
        /**
         * IO 线程 mainLoop accept fd <==  channel subloop
         * mainLoop 事先注册一个回调 cb（需要 subloop 来执行） wakeup subloop 之后执行下面的方法，
         * 执行之前 mainLoop 注册的 cb
         */
        doPendingFunctors();
    }

    LOG_INFO("EventLoop::loop() - %p stop looping.", this);
    looping_ = false;
}

/**
 * 退出事件循环
 * 1. loop 在自己的线程中调用 quit
 * 2. 在非 loop 的线程中调用 loop 的 quit
 *
 * moduo 库没有使用「安全队列」而是通过 eventfd 来通知 subLoop
 *            mainLoop
 *    (生产者-消费者的线程安全队列)
 * subLoop1     subLoop2    subLoop3
 */
void EventLoop::quit() {
    quit_ = true;

    // 2. 在其他线程中调用 quit，例如在一个 subloop(worker) 中调用了 mainLoop(IO) 的 quit
    if (!isInLoopThread()) {
        wakeup();
    }
}

// 在当前 loop 中执行 cb
void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) {  // 在当前的 loop 线程中执行 cb
        cb();
    } else {  // 在非当前 loop 线程中执行 cb，就需要唤醒 loop 所在线程，执行 cb
        queueInLoop(cb);
    }
}

// 把 cb 放入队列中，唤醒 loop 所在的线程，执行 cb
void EventLoop::queueInLoop(Functor cb) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    // 唤醒相应的，需要执行上面回调操作的 loop 的线程了
    //!NOTE: callingPendingFunctors_ 当前 loop 正在执行回调，但是 loop 又有了新的回调，因此还需要唤醒 poller 以便再次执行
    if (!isInLoopThread() || callingPendingFunctors_) {
        wakeup();  // 唤醒 loop 就在线程
    }
}

// 用来唤醒 loop 所在的线程，向 wakeupFd_ 写一个数据，wakeupChannel 就发生读事件，当前 loop 线程就会被唤醒
void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        LOG_ERROR("EventLoop::wakeup() - writes %lu bytes instead of 8", n);
    }
}

// 调用 poller->updateChannel
void EventLoop::updateChannel(Channel *channel) { poller_->updateChannel(channel); }

// 调用 poller->removeChannel
void EventLoop::removeChannel(Channel *channel) { poller_->removeChannel(channel); }

// 调用 poller->hasChannel
void EventLoop::hasChannel(Channel *channel) { poller_->hasChannel(channel); }

void EventLoop::handleRead() {
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if (n != sizeof(one)) {
        LOG_ERROR("EventLoop::handleRead() - reads %ld bytes instead of 8", n);
    }
}

// 执行回调
void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);  // 解放 pendingFunctors_，减少时延
    }

    for (const Functor &functor : functors) {
        functor();  // 执行当前
    }

    callingPendingFunctors_ = false;
}