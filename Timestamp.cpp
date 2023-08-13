#include "Timestamp.h"

#include <time.h>

Timestamp::Timestamp() : secondsSinceEpoch_(0) {}

Timestamp::Timestamp(int64_t microSecondsSinceEpoch) : secondsSinceEpoch_(microSecondsSinceEpoch) {}

Timestamp Timestamp::now() { return Timestamp(time(NULL)); }

std::string Timestamp::toString() const {
    char buf[128] = {0};
    //!NOTE: localtime 参数是秒数，time(NULL) 返回的也是秒数
    tm *tm_time = localtime(&secondsSinceEpoch_);
    snprintf(buf,
             128,
             "%4d/%02d/%02d %02d:%02d:%02d",
             tm_time->tm_year + 1900,
             tm_time->tm_mon + 1,
             tm_time->tm_mday,
             tm_time->tm_hour,
             tm_time->tm_min,
             tm_time->tm_sec);
    return buf;
}

// int main()
// {
//     std::cout << Timestamp::now().toString() << std::endl;
//     return 0;
// }
