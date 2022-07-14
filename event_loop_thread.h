#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>

#include "noncopyable.h"
#include "thread.h"

/**
 * EventLoopThread 绑定了一个 loop 跟一个 thread,
 * 让这个 loop 运行在 Thread 里面, 在 Thread 里面去创建一个 loop,
 *
 */

class EventLoop;

class EventLoopThread : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

public:
    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                    const std::string &name = "");
    ~EventLoopThread();

public:
    EventLoop *startLoop();

private:
    void threadFunc();

private:
    EventLoop *loop_ /* __attribute__((mutex_))  */;
    bool exiting_;
    Thread thread_;
    std::mutex  mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;
};
