#pragma once

/**
 * noncopyable 被继承以后，派生类对象可以正常的构造和析构，
 * 但是派生类对象进行无法拷贝构造和赋值操作
 */

class noncopyable {
  public:
    noncopyable(const noncopyable &) = delete;
    noncopyable &operator=(const noncopyable &) = delete;

  protected:
    noncopyable() = default;
    ~noncopyable() = default;
};