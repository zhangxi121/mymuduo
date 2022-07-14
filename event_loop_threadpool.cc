#include "event_loop_threadpool.h"

#include "event_loop.h"
#include "event_loop_thread.h"

EventLoopThreadpool::EventLoopThreadpool(EventLoop *baseLoop, const std::string &nameArg)
    : baseLoop_(baseLoop),
      name_(nameArg),
      started_(false),
      numThreads_(0),
      next_(0)
{
}

EventLoopThreadpool::~EventLoopThreadpool()
{
    // Don't delete loop, it's stack variable,
}

void EventLoopThreadpool::start(const ThreadInitCallback &cb)
{
    if (!baseLoop_->isInLoopThread())
    {
        abort();
    }
    started_ = true;

    for (int i = 0; i < numThreads_; ++i)
    {
        char buf[name_.size() + 32] = {0};
        snprintf(buf, sizeof(buf), "%s%d", name_.c_str(), i);
        EventLoopThread *t = new EventLoopThread(cb, buf);
        threads_.emplace_back(std::unique_ptr<EventLoopThread>(t));
        loops_.emplace_back(t->startLoop()); // t->startLoop() 底层创建线程, 绑定一个新的 EventLoop, 并返回该 loop 的地址,
    }

    // 没有设置 setThreadPool(), 整个服务端只有一个线程运行着 baseLoop_,
    if (0 == numThreads_ && cb)
    {
        cb(baseLoop_);
    }
}

// 如果工作在多线程中, baseLoop_ 默认以轮询的方式分配 Channel 给 subLoop,
EventLoop *EventLoopThreadpool::getNextLoop()
{
    EventLoop *loop = baseLoop_;

    // 这里是 if, 一次获取一个 loop, 通过轮询获取下一个处理事件的 loop,
    if (!loops_.empty())
    {
        loop = loops_[next_];
        ++next_;

        if (next_ >= loops_.size())
        {
            next_ = 0;
        }
    }
    return loop;
}

std::vector<EventLoop *> EventLoopThreadpool::getAllLoops()
{
    if (loops_.empty())
    {
        return std::vector<EventLoop *>(1, baseLoop_);
    }
    else
    {
        return loops_;
    }
}
