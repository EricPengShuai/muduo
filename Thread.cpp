#include "Thread.h"

#include "CurrentThread.h"

#include <semaphore.h>

std::atomic_int Thread::numCreated_(0);  // 静态成员类外初始化

Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false), joined_(false), tid_(0), func_(std::move(func)), name_(name) {
    setDefaultName();
}

Thread::~Thread() {
    if (started_ && !joined_) {
        thread_->detach();  // thread 类提供了设置分离线程的方法
    }
}

// 开启线程，一个 Thread 对象记录的就是一个新线程的详细信息
void Thread::start() {
    started_ = true;

    sem_t sem;
    sem_init(&sem, false, 0);  // 初始化信号量为 0

    // 开启线程
    thread_ = std::shared_ptr<std::thread>(new std::thread([&]() {
        tid_ = CurrentThread::tid();  // 获取线程的 tid 值
        sem_post(&sem);
        func_();  // 开启一个新线程，专门执行该线程函数
    }));

    //!NOTE: 这里必须等待获取上面新创建的线程的 tid
    sem_wait(&sem);
}

void Thread::join() {
    joined_ = true;
    thread_->join();
}

void Thread::setDefaultName() {
    //!NOTE: 这是一个原子变量，重载了 ++
    int num = ++ numCreated_;
    if (name_.empty()) {
        char buf[32] = {0};
        snprintf(buf, sizeof(buf), "Thread%d", num);
        name_ = buf;
    }
}
