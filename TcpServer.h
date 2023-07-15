#pragma once

/**
 * 用户使用 muduo 编写服务器程序
 */
#include "Acceptor.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"

#include <functional>
#include <memory>
#include <string>
#include <atomic>
#include <unordered_map>

class TcpServer : noncopyable {
  public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    // 是否重用端口
    enum Option { kNoReusePort, kReusePort };

    TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string &nameArg, Option option = kNoReusePort);
    ~TcpServer();

    void setThreadInitCallback(const ThreadInitCallback &cb) { threadInitCallback_ = cb; }
    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }

    void setThreadNum(int numThreads);  // 设置底层 subLoop 的个数

    void start();  // 开启服务器监听

  private:
    void newConnection(int sockfd, const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionPtr &conn);
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    EventLoop *loop_;  // baseLoop 用户定义的 loop

    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_;  // 运行在 mainLoop，任务就是监听新连接事件

    std::shared_ptr<EventLoopThreadPool> threadPool_;  // one loop per thread

    ConnectionCallback connectionCallback_;        // 有新连接时的回调
    MessageCallback messageCallback_;              // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_;  // 消息发送完成之后的回调

    ThreadInitCallback threadInitCallback_;  // loop 线程初始化的回调
    std::atomic_int started_;

    int nextConnId_;
    ConnectionMap connections_;  // 保存所有连接
};
