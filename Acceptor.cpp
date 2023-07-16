#include "Acceptor.h"

#include "InetAddress.h"
#include "Logger.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static int createNonblocking() {
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (sockfd < 0) {
        LOG_FATAL("Acceptor [static]:%s:%d listenfd create error: %d", __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop)
    , acceptSocket_(createNonblocking())  // socket
    , acceptChannel_(loop, acceptSocket_.fd())
    , listening_(false) 
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(reuseport);
    acceptSocket_.bindAddress(listenAddr);  // bind

    //!NOTE: TcpServer::start() 中 Acceptor.listen 有新用户连接，要执行一个回调
    // baseLoop => acceptChannel_(listenfd) =>
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor() {
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen() {
    listening_ = true;
    acceptSocket_.listen();         // listen
    acceptChannel_.enableReading(); // acceptChannel_ => Poller
}

// listenfd 有事件发生了，就是有新用户连接了
void Acceptor::handleRead() {
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >= 0) {
        if (newConnectionCallback_) {  // 轮询找到 subLoop，唤醒分发当前的新客户端的 Channel
            newConnectionCallback_(connfd, peerAddr);
        } else {
            ::close(connfd);
        }
    } else {
        LOG_ERROR("Acceptor::handleRead() accept error: %d", errno);
        if (errno == EMFILE) {
            LOG_ERROR("Acceptor::handleRead() sockfd reached limit!");
        }
    }
}
