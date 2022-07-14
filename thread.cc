#include <semaphore.h>

#include "thread.h"

#include "current_thread.h"

std::atomic_int Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false),
      joined_(false),
      thread_(nullptr),
      tid_(0),
      func_(std::move(func)),
      name_(name)
{

    setDefaultName();
}

Thread::~Thread()
{
    if (started_ && !joined_)
    {
        thread_->detach();
    }
}

// 一个 Thread 对象就是记录的一个新线程的详细信息, started_  joined_  tid_   name_ 等详细信息,
void Thread::start()
{
    started_ = true;

    sem_t sem;
    sem_init(&sem, 0, 0);

    // func_() 线程函数,
    auto f = [&]()
    {
        tid_ = CurrentThread::tid();
        sem_post(&sem);
        // 这个函数包含了 EventLoop,
        func_();
    };

    // 开启一个新线程, 专门执行 func_() 线程函数,
    thread_ = std::make_shared<std::thread>(f);

    // 这里必须等待 获取上面的新创建的线程的 tid 值, 获取到tid再返回退出,
    sem_wait(&sem);
}

void Thread::join()
{
    joined_ = true;
    thread_->join();
}

void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if (name_.empty())
    {
        char buf[32] = {0};
        snprintf(buf, sizeof(buf), "Thread%d", num);
        name_ = buf;
    }
}
