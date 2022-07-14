#include "event_loop_thread.h"

#include "event_loop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb, const std::string &name)
    : loop_(nullptr),
      exiting_(false),
      thread_(std::bind(&EventLoopThread::threadFunc, this), name),
      mutex_(),
      cond_(),
      callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != nullptr)
    {
        loop_->quit();
        thread_.join();
    }
}

EventLoop *EventLoopThread::startLoop()
{
    thread_.start();

    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (loop_ == nullptr)
        {
            cond_.wait(lock);
        }
        loop = loop_;
    }
    return loop;
}

// 下面这个方法是在单独的新线程里面运行的,
void EventLoopThread::threadFunc()
{
    // 创建一个独立的 eventloop, 和上面的线程是一一对应的, "one loop per thread" ,
    EventLoop loop;

    if (callback_)
    {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop(); // EventLoop.loop() ==> Poller.poll()

    // loop 返回了, Poller 返回了, 不再进行事件循环了,
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}
