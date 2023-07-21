#pragma once

#include <iostream>
#include <string>

// 时间类
class Timestamp {
  private:
    int64_t secondsSinceEpoch_;

  public:
    Timestamp();
    explicit Timestamp(int64_t secondsSinceEpoch);

    static Timestamp now();
    std::string toString() const;
};
