#pragma once

#include "noncopyable.h"

#include <string>

// 定义日志级别
enum LogLevel {
    INFO,   // 普通信息
    ERROR,  // 错误信息
    FATAL,  // core 信息
    DEBUG,  // 调试信息
};

// 单例: 输出一个日志类, 默认是 private 继承 noncopyable
class Logger : noncopyable {
  private:
    Logger(/* args */) {}
    int logLevel_;

  public:
    static Logger &instance();  // 获取日志唯一实例对象

    void setLogLevel(int level);  // 设置日志级别

    void log(std::string msg);  // 写日志
};

#define LOG_INFO(logmsgFormat, ...)                                                                                    \
    do {                                                                                                               \
        Logger &logger = Logger::instance();                                                                           \
        logger.setLogLevel(INFO);                                                                                      \
        char buf[1024];                                                                                                \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);                                                              \
        logger.log(buf);\
    } while (0)

#define LOG_ERROR(logmsgFormat, ...)                                                                                   \
    do {                                                                                                               \
        Logger &logger = Logger::instance();                                                                           \
        logger.setLogLevel(ERROR);                                                                                     \
        char buf[1024];                                                                                                \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);                                                              \
        logger.log(buf);\
    } while (0)

#define LOG_FATAL(logmsgFormat, ...)                                                                                   \
    do {                                                                                                               \
        Logger &logger = Logger::instance();                                                                           \
        logger.setLogLevel(FATAL);                                                                                     \
        char buf[1024];                                                                                                \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);                                                              \
        logger.log(buf);\
        exit(-1);                                                                                                      \
    } while (0)

#ifdef MUDEBUG
#define LOG_DEBUG(logmsgFormat, ...)                                                                                   \
    do {                                                                                                               \
        Logger &logger = Logger::instance();                                                                           \
        logger.setLogLevel(DEBUG);                                                                                     \
        char buf[1024];                                                                                                \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);                                                              \
        logger.log(buf);\
    } while (0)
#else
#define LOG_DEBUG(logmsgFormat, ...)
#endif
