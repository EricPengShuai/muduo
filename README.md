## Muduo Learning

### 1、代码梳理——独立部分

#### noncopyable
- delete 拷贝构造和赋值构造
- default 正常构造和析构

#### Timestamp
- 封装 time(NULL) 
- 提供 now() 和 toString() 方法

### 2、代码梳理——核心代码

#### Channel
- 封装 fd, events, revents
    - fd: poller 监听的 fd，一种是 listenfd，一种是 connfd
    - events: fd 感兴趣的事件
    - revents: poller 返回的具体事件，被 EPollPoller::fillActiveChannels 具体设置

- 一系列的 callbacks

#### Poller 和 EPollPoller - DeMultiplex
- Poller 是基类，提供具体的接口的抽象类
    - unordered_map<int, Channel*> channelMaps 保存被监听的 fd 列表
- EPollPoller，继承 Poller，默认维护大小为 16 的 vector events_
    - poll -> epoll_wait
    - update -> updateChannel -> epoll_ctl

#### EventLoop - Reator
- 管理 channels 和 poller
    - ChannelList activateChannels_
    - unique_ptr poller_

- wakeupFd 以及 unique_ptr 管理的 wakeupChannel
    - eventfd() 系统调用实现线程之间的通信
    - 当 mainLoop 获取一个新用户的 connfd，通过轮询算法选择一个 subloop，通过该成员唤醒subloop 处理 channel

- runInLoop: 在当前 loop 中执行回调
- queueInLoop: 通过 wakeup() 唤醒对应的 loop 执行回调

#### Thread 和 EventLoopThread

- Thread 类利用 thread 头文件封装了线程的 join, start 等方法
    - start 方法新开一个线程执行 func 回调，其中使用 semaphore 等待 tid 的生成
    - 使用智能指针管理 thread_ 对象

- EventLoopThread 完美体现 one loop per thread
    - startLoop 开启一个事件循环，创建新线程
        - 包含 Thread 对象 thread_ 并通过 bind 绑定自己的 ThreadFunc 函数
        - 线程执行函数 ThreadFunc 每次执行都会创建一个 EventLoop 对象

#### EventLoopThreadPool
- 管理 EventLoopThread 以及 EventLoop，vector 
- start 方法创建 numThreads_ 个线程，并获取对应的 loop, one loop per thread，分别存储在 threads_ 和 loops_ 中，底层调用 EventLoopThread::startLoop 创建 loop
- getNextLoop 方法轮询获取下一个 subLoop

#### Socket
- 封装了 socket 操作：bind listen accept
- 提供 shutdownWrite() 关闭写端

#### Acceptor
- 主要封装 listenfd 的相关操作
- acceptSocket_ 以及 acceptChannel_, 设置回调，监听新用户
    - 主要关注 channel 的 readCallback，绑定自己的 handleRead 函数
    - handleRead 中通过上层设置 newConnectionCallback_ 处理新用户的 connfd

#### Buffer
- 缓冲区，nonblocking IO
- 应用写数据 -> buffer -> Tcp 发送缓冲区 -> send
- 通过 prependable | readerIndex | writerIndex 思想实现

#### TcpConnection
- 一个连接成功的客户端包含一个 TcpConnection
- 设置 connfdChannel 的回调，包括读写、错误、关闭等，acceptChannel 只关注读的回调
- 回调绑定的都是自己的 handleRead(), handleWrite(), handleClose(), handleError() 函数
- 发送数据 send, 实际使用 sendInLoop 发送数据，因为如果应用写的快，而内核发送数据慢，需要把发送数据写入缓冲区

#### TcpServer
- 最上层的类，提供给用户使用 muduo 编写服务器程序
- 管理 Acceptr, 设置 newConnectionCallback 回调
    - Acceptor: 创建一个非阻塞的 listenfd，socket bind listen 然后绑定 handleRead 获取新用户的 connfd
    - handleRead 中使用 newConnectionCallback 回调
- 管理 EventLoopThreadPool, 设置底层线程数量，不包括 baseLoop
    - ConnectionMap connections_

### 3、简单例子
参考：[test_mymuduo.cpp](./example/test_mymuduo.cpp)
```cpp
EventLoop loop;
InetAddress addr(8080);
EchoServer server(&loop, addr, "EchoServer-01"); // Acceptor non-blocking listenfd create bind
server.start(); // listen  loopthread  listenfd => acceptChannel => mainLoop =>
loop.loop(); // 启动 mainLoop 的底层 Poller
```

### 4、一键部署

```sh
# 正常模式
sudo ./autobuild.sh 

# DEBUG 模式
sudo ./autobuild.sh DEBUG
```

### 5、亮点

#### eventfd()
- mainLoop 和 subLoop 之间没有使用同步队列，没有使用生产者消费者模型，而是使用 eventfd() 创建 wakeupFd 作为线程之间的通知唤醒逻辑，效率是很高的
- Libevent 中使用 socketpair 基于 AF_UNIX 创建双向管道用于线程之间的通信

#### multiple reators
- mainReators 和 subReator，实际上是 mainLoop 和 subLoop，包括 Channel 和 Poller
- EventLoop 就是图中的 Reactor 和 Demultiplex
![reactor](./images/reactor.png)

#### C++11
- 使用 C++11 改写原有 muduo 库，不依赖 boost 库

### 6、整体流程梳理

![muduo](./images/muduo.png)

### 7、参考

- 施磊 ————【高级】手写C++ Muduo网络库项目-掌握高性能网络库实现原理