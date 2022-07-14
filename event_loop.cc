#include <algorithm>
#include <errno.h>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "event_loop.h"

#include "channel.h"
#include "logger.h"

namespace
{
    // 防止一个线程创建多个 EventLoop, 全局的 EventLoop* 变量,
    // thread_local 的机制, 每个线程在访问 t_loopInThisThread 时候, 每个线程都有一个 t_loopInThisThread 副本,
    // 每个线程都有一个 EventLoop*,
    __thread EventLoop *t_loopInThisThread = nullptr;
    const int kPollTimeMs = 10000; // 定义默认的 Poller IO复用接口的超时时间, 10s钟,

    // 创建 wakeupFd, 用来 notify 唤醒 subReactor, 处理新来的 Channel,
    int createEventFd()
    {
        int evefd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (evefd < 0)
        {
            LOG_FATAL("eventfd error : %d \n", errno);
            abort();
        }
        return evefd;
    }
}

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      threadId_(CurrentThread::tid()),
      poller_(Poller::newDefaultPoller(this)), // 智能指针自动析构,
      wakeupFd_(createEventFd()),
      wakeupChannel_(new Channel(this, wakeupFd_)), // 智能指针自动析构,
                                                    //   currentActiveChannel_(nullptr),
      callingPendingFunctors_(false)
{
    LOG_DEBUG("EventLoop created %p int thread %d", this, threadId_);
    if (t_loopInThisThread)
    {
        LOG_FATAL("Another EventLoop %p exists in this thread is %d\n", t_loopInThisThread, threadId_);
        exit(1);
    }
    else
    {
        t_loopInThisThread = this;
    }

    // 设置 wakeupFd_ 感兴趣的事件类型, 以及发生事件后的回调操作,
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 每一个 EventLoop 都将监听 wakeupChannel 的 EPOLLIN 读事件,
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::loop()
{
    if (!isInLoopThread())
        abort();
    looping_ = true;
    quit_ = false;
    LOG_INFO("EventLoop %p start looping!\n", this);

    while (!quit_)
    {
        activeChannels_.clear();

        // Poll 主要监听两类 fd, 一种是 clientFd, 一种是wakeuoFd,
        // clientFd 绑定的 Channel 去完成  channel->pollReturnTime_); 是被动调用回调,
        // wakeupFd 绑定的 Channel 其实没做啥事, 但是唤醒了 wakeFd 就可以主动的去执行 subLoop->doPendingFunctors(),
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);

        for (Channel *channel : activeChannels_)
        {
            // Poller 能够监听哪些 Channel 发生事件了, 然后上报给 EventLoop, 然后 EventLoop 通知 Channel 处理相应的事件,
            channel->handleEvent(pollReturnTime_);
        }

        // 执行当前 EventLoop 事件循环需要处理的回调操作,
        /**
         * IO线程 mainloop accept() 的工作, 得到客户端的 fd, 打包 "fd + channel",
         * 已连接的 Channel 得分发给 subLoop,
         * mainLoop 事先注册一个 callback() , 这个 callback() 需要 subLoop 执行,
         * 但是此时的 subLioop 又阻塞在 poll() 函数上了, mainLoop 通过 eventfd 唤醒 subLoop 以后, poller_->poll() 唤醒 subLoop 以后,
         * 因为之前注册的 eventFd 的事件其实啥也没做, 所以  eventFd->channel->handleEvent(pollReturnTime_) 其实没做啥事情,
         * 当然此时还有其他的 Fd 被唤醒, channel->handleEvent(pollReturnTime_);
         * 下面为什么还需要 doPendingFunctors() 是处理自己的一些回调, 这些回调并没有注册到 Poller 上, 所以与上面的 channel->handleEvent(pollReturnTime_) 是两种不同的回调,
         * 唤醒之后, 回调做什么事情呢, mainLoop 事先给我注册一个回调, 然后唤醒 subLoop, 让 subLoop 去执行下面的方法(都是mainLoop注册的回调),
         * 执行之前 mainLoop 注册的所有 callback 操作,
         */
        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop looping!\n", this);
    looping_ = false;
}

// 退出事件循环,
// 1.loop在自己的线程中调用 quit(),
// 2.在其他线程中调用的 quit(), 在一个 subLoop(worker线程) 中调用了 mainLoop(IO线程) 的 quit(),
// 在非 loop的线程中调用 loop 的 quit(), 把 mainLoop->quit_ = true, 然后在 loop() { while(!quit_) ...} 循环中跳出了,
void EventLoop::quit()
{
    quit_ = true;

    // 在其他线程中调用的 quit(), 在一个 subLoop(worker线程) 中调用了 mainLoop(IO线程) 的 quit(),
    if (!isInLoopThread())
    {
        wakeup();
    }
}

void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread())
    {
        cb();
    }
    else
    {
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    // 唤醒相应的需要执行上面回调的 loop 线程,
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup(); // 唤醒 loop 所在线程,
    }
}

// 唤醒 loop 所在的线程, 向 wakeupFd 写一个数据, 来唤醒 wakeup,
// 那么 wakeupChannel 就发生读事件,当前 loop 线程就会被唤醒,
void EventLoop::wakeup()
{
    if (!isInLoopThread())
    {
        uint64_t one = 1;
        size_t n = ::write(wakeupFd_, &one, sizeof one);
        if (n != sizeof one)
        {
            LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8!\n", n);
        }
    }
}

void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}

void EventLoop::hasChannel(Channel *channel)
{
    poller_->hasChannel(channel);
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = ::read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8\n", n);
    }
}

void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (const Functor &functor : functors)
    {
        functor();  // 执行当前 loop 需要执行的回调操作,  callingPendingFunctors_ 控制当前 loop 在执行回调,
    }
    callingPendingFunctors_ = false;
}
