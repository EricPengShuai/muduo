#pragma once

#include "noncopyable.h"

class InetAddress;

// 封装 socket fd
class Socket : noncopyable {
  public:
    explicit Socket(int sockfd) : sockfd_(sockfd) {}

    ~Socket();

    int fd() const { return sockfd_; }
    void bindAddress(const InetAddress &localaddr);
    void listen();
    int accept(InetAddress *peeraddr);

    void shutdownWrite();

    // 设置 sockfd 选项
    void setTcpNoDelay(bool on);
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setKeepAlive(bool on);

  private:
    const int sockfd_;
};
