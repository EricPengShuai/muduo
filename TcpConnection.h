#pragma once

#include "Buffer.h"
#include "Callbacks.h"
#include "InetAddress.h"
#include "Timestamp.h"
#include "noncopyable.h"

#include <atomic>
#include <memory>
#include <string>

class Channel;
class EventLoop;
class Socket;

/**
 * TcpServer => Acceptor => 有一个新用户连接，通过 accept 函数那道 connfd
 * => TcpConnection 设置回调 => Channel => Poller => Channel 执行回调
 */
class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection> {
  public:
    TcpConnection(EventLoop *loop,
                  std::string &nameArg,
                  int sockfd,
                  const InetAddress &localAddr,
                  const InetAddress &peerAddr);
    ~TcpConnection();

    EventLoop *getLoop() const { return loop_; }
    const std::string &name() const { return name_; }
    const InetAddress &localAddress() const { return localAddr_; }
    const InetAddress &peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }

    void send(std::string &buf);  // 发送数据

    void shutdown();  // 关闭连接

    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }

    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }

    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }

    void setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark) {
        highWaterMarkCallback_ = cb;
        highWaterMark_ = highWaterMark;
    }

    void setCloseCallback(const CloseCallback &cb) { closeCallback_ = cb; }

    void connectEstablished();  // 连接建立
    void connectDestroyed();    // 连接销毁

  private:
    enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };  // 连接状态

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void setState(StateE s) { state_ = s; }

    void sendInLoop(const void *message, size_t len);
    void shutdownInLoop();

    EventLoop *loop_;  // 这里绝对不是 baseLoop，因为 TcpConnection 都是在 subLoop 里面管理的
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    // 和 Acceptor 类似: Acceptor => mainLoop | TcpConnection => subLoop
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    // 这些都是上层 TcpServer 设置的，TcpServer 中有些是用户设置的
    ConnectionCallback connectionCallback_;        // 有新连接时的回调
    MessageCallback messageCallback_;              // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_;  // 消息发送完成之后的回调
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;

    size_t highWaterMark_;

    Buffer inputBuffer_;
    Buffer outputBuffer_;
};
