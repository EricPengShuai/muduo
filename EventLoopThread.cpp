#include "EventLoopThread.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb, const std::string &name)
    : loop_(nullptr)
    , exiting_(false)
    , thread_(std::bind(&EventLoopThread::threadFunc, this), name)
    , mutex_() // 默认构造
    , cond_()
    , callback_(cb) {}

EventLoopThread::~EventLoopThread() {
    exiting_ = true;
    if (loop_ != nullptr) {
        loop_->quit();
        thread_.join();
    }
}

EventLoop *EventLoopThread::startLoop() {
    thread_.start();  // 启动底层的新线程

    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (loop_ == nullptr) {
            cond_.wait(lock);  // 有一个线程之间通信的操作
        }
        loop = loop_;
    }
    return loop;
}

// 下面这个方法在单独的新线程里面运行
void EventLoopThread::threadFunc() {
    // 创建一个独立的 EventLoop，和上面的线程是一一对应的，one loop per thread
    EventLoop loop;

    if (callback_) {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop();  // EventLoop loop => Poller->poll
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}
