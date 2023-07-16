#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

/**
 * !NOTE: 从 fd 读数据，相当于读到 buffer 的写缓冲区
 * 从 fd 上读数据，Poller 工作在 LT 模式
 * Buffer 缓冲区是有大小的，但是从 fd 上读取数据的时候却不知道 tcp 数据最终的大小
 */
ssize_t Buffer::readFd(int fd, int *saveErrno) {
    char extrabuf[65536] = {0};  // 栈上分配的内存空间 64K

    struct iovec vec[2];  // iovec 结构体包含起始地址以及对应长度

    const size_t writable = writableBytes();  // buffer 剩余可写空间大小
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[2].iov_len = sizeof(extrabuf);

    // 相当于一次最多读 64K 的数据
    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
    const size_t n = ::readv(fd, vec, iovcnt);
    if (n < 0) {
        *saveErrno = errno;
    } else if (n <= writable) {  // buffer 可写缓冲区够存放
        writerIndex_ += n;
    } else {
        // buffer 可写缓冲区不够存放，extrabuf 写入了数据
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable);  // 从 writerIndex_ 开始写剩余的数据
    }
    return n;
}

Buffer::~Buffer() {}

//!NOTE: 向 fd 写数据，相当于就是从 buffer 读缓存区拿数据
ssize_t Buffer::writeFd(int fd, int *saveErrno) {
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0) {
        *saveErrno = errno;
    }
    return n;
}
