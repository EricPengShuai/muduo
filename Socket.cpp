#include "Socket.h"

#include "InetAddress.h"
#include "Logger.h"

#include <netinet/tcp.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

Socket::~Socket() { ::close(sockfd_); }

void Socket::bindAddress(const InetAddress &localaddr) {
    if (0 != ::bind(sockfd_, (const sockaddr *)localaddr.getSockAddr(), sizeof(sockaddr_in))) {
        LOG_FATAL("Socket::bindAddress - bind sockfd: %d fail", sockfd_);
    }
}

void Socket::listen() {
    if (0 != ::listen(sockfd_, 1024)) {
        LOG_FATAL("Socket::listen() - listen sockfd: %d fail", sockfd_);
    }
}

int Socket::accept(InetAddress *peeraddr) {
    /**
     * 1. accept 参数中 len 必须初始化
     * 2. 对返回的 coonfd 没有设置非阻塞
     * Reactor 模型 one loop per thread
     * poller + non-blocking io
     */
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    bzero(&addr, sizeof(addr));
    int connfd = ::accept4(sockfd_, (sockaddr *)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (connfd >= 0) {
        peeraddr->setSockAddr(addr);
    }
    return connfd;
}

// 关闭写端
void Socket::shutdownWrite() {
    if (::shutdown(sockfd_, SHUT_WR) < 0) {
        LOG_ERROR("Socket::shutdownWrite() - shutdownWrite error");
    }
}

// Enable/disable TCP_NODELAY (Nagle's algorithm)
void Socket::setTcpNoDelay(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

// Enable/disable SO_REUSEADDR
void Socket::setReuseAddr(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

// Enable/disable SO_REUSEPORT
void Socket::setReusePort(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
}

// Enable/disable SO_KEEPALIVE
void Socket::setKeepAlive(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}