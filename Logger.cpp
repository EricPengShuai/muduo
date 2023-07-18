#include "Logger.h"

#include "Timestamp.h"

#include <iostream>

Logger &Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::setLogLevel(int level) { logLevel_ = level; }

void Logger::log(std::string msg) {
    std::string pre = "";
    switch (logLevel_) {
        case INFO:
            pre = "[INFO] ";
            break;
        case ERROR:
            pre = "[ERROR] ";
            break;
        case FATAL:
            pre = "[FATAL] ";
            break;
        case DEBUG:
            pre = "[DEBUG] ";
            break;
        default:
            break;
    }
    //!NOTE: 将日志打印由两次改为一次，避免并发时打印错位
    std::cout << pre + Timestamp::now().toString() << ": " << msg << std::endl;
}
