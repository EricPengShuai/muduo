#pragma once

#include "Channel.h"
#include "Socket.h"
#include "noncopyable.h"

class EventLoop;
class InetAddress;

class Acceptor : noncopyable {
  public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress &)>;

    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback &cb) { newConnectionCallback_ = cb; }

    bool listening() const { return listening_; }
    void listen();

  private:
    void handleRead();

    EventLoop *loop_;
    Socket acceptSocket_;
    Channel acceptChannel_;

    // 将 accept 到的 connfd 绑定到 channel 上并注册事件，由上层 TcpServer 设置回调
    NewConnectionCallback newConnectionCallback_;
    bool listening_;
};
