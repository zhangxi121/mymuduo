#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "noncopyable.h"

class EventLoop;
class EventLoopThread;

class EventLoopThreadpool : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

public:
    EventLoopThreadpool(EventLoop *baseLoop, const std::string &nameArg);
    ~EventLoopThreadpool();

public:
    void setThreadNum(int numThreads) { numThreads_ = numThreads; }
    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    /**
     * 如果工作在多线程中, baseLoop_ 默认以轮询的方式分配 Channel 给 subLoop,
     */
    EventLoop *getNextLoop();

    std::vector<EventLoop *> getAllLoops();

    bool started() const { return started_; }
    const std::string &name() const { return name_; }

private:
    /**
     * 用户没有使用 tcpserver->setThreadNum() 来设置底层的数量的时候, 那么 muduo 使用的就是单线程模型型,
     * baseLoop_ 就是用户一开始创建的loop, 对应的用户线程,
     * 这个线程不仅仅作为 新用户的连接, 还要作为已连接用户的读写事件, 所以需要设置线程数量, 就会产生新的 EventLoop,
     */
    EventLoop *baseLoop_;
    std::string name_;
    bool started_;
    int numThreads_;
    int next_;
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop *> loops_;
};
