#include "TcpServer.h"
#include "Logger.h"

#include <functional>
#include <strings.h>

static EventLoop *CheckLoopNotNull(EventLoop *loop) {
    if (loop == nullptr) {
        LOG_FATAL("TcpServer [static]CheckLoopNotNull - mainLoop is null!");
    }
    return loop;
}

TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string &nameArg, Option option)
    : loop_(CheckLoopNotNull(loop))
    , ipPort_(listenAddr.toIpPort())
    , name_(nameArg)
    , acceptor_(new Acceptor(loop, listenAddr, option == kReusePort))
    , threadPool_(new EventLoopThreadPool(loop, name_))
    , connectionCallback_()
    , messageCallback_()
    , nextConnId_(1) 
    , started_(0)
{
    // 当有新用户连接时，会执行 TcpServer::newConnection 回调
    acceptor_->setNewConnectionCallback(
        std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer() {
    for (auto &item : connections_) {
        // 这个局部的 shared_ptr 智能指针对象，出右括号，可以自动释放 new 出来的 TcpConnection 对象资源
        TcpConnectionPtr conn(item.second);
        item.second.reset();

        // 销毁链接
        conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

// 设置底层 subLoop 的个数
void TcpServer::setThreadNum(int numThreads) { threadPool_->setThreadNum(numThreads); }

// 开启服务器监听 loop.loop()
void TcpServer::start() {
    if (started_++ == 0)  // 防止一个 TcpServer 对象被 start 多次
    {
        threadPool_->start(threadInitCallback_);                          // 启动底层的 loop 线程池
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));  // listen
    }
}

// 有一个新的客户端的连接，acceptor 会执行这个回调, sockfd 就是 connfd
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr) {
    // 轮询算法，选择一个 subLoop 来管理 channel
    EventLoop *ioLoop = threadPool_->getNextLoop();

    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;  // 这个不涉及线程安全问题

    std::string connName = name_ + buf;
    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s",
             name_.c_str(),
             connName.c_str(),
             peerAddr.toIpPort().c_str());

    // 通过 sockfd 获取其绑定的本机的 ip 地址和端口信息
    sockaddr_in local;
    ::bzero(&local, sizeof(local));
    socklen_t addrLen = sizeof(local);
    if (::getsockname(sockfd, (sockaddr *)&local, &addrLen) < 0) {
        LOG_ERROR("TcpServer::newConnection - getsockname sockets::getLocalAddr");
    }
    InetAddress localAddr(local);

    // 根据连接成功的 sockfd，创建 TcpConnection 连接对象
    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
    connections_[connName] = conn;

    // 下面的回调都是用户设置给 TcpServer => TcpConnection => Channel => Poller => notify channel 调用回调
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    // 设置了如何关闭连接的回调
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1)
    );

    // 直接调用 TcpConnection::connectEstablished
    // 1. 设置了 threadNum 就会进入 queueInLoop <-- subLoop
    // 2. 没有设置 threadNum 就直接进入 runInLoop 的 cb() <-- baseLoop
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

//!NOTE: 这里 TcpConnectionPtr 别写错成了 TcpConnection
void TcpServer::removeConnection(const TcpConnectionPtr &conn) {
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn) {
    LOG_INFO("TcpServer::removeConnectionInLoop - name [%s], connection [%s]", name_.c_str(), conn->name().c_str());

    connections_.erase(conn->name());

    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}