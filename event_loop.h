#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "current_thread.h"
#include "noncopyable.h"
#include "poller.h"
#include "timestamp.h"

class Channel;
class Poller;

// 事件循环类, Channel 、 Poller(epoll的抽象),
class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;

public:
    EventLoop();
    ~EventLoop();

public:
    // 开启事件循环,
    void loop();

    // 退出事件循环,
    // 1.loop在自己的线程中调用 quit(),
    // 2.在其他线程中调用的 quit(), 在一个 subLoop(worker线程) 中调用了 mainLoop(IO线程) 的 quit(),
    void quit();

    Timestamp pollReturnTime() const { return pollReturnTime_; }

    // 如果 cb 相关联的 Channel 在当前 loop当中, 在当前 loop 中执行 cb,
    void runInLoop(Functor cb);
    // 如果 cb 相关联的 Channel 不在当前 loop当中, 就需要去唤醒 loop 所在的线程, 执行cb,
    // 把 cb 返给到队列中, 唤醒 loop 所在的线程, 执行 cb, 如 subLoop2 里面去执行了 subLoop3 的 cb,
    void queueInLoop(Functor cb);

    // 唤醒 loop 所在的线程, 向 wakeupFd 写一个数据, 来唤醒 wakeup,
    void wakeup();

    // Chanenl.updateChannel() ==>  EventLoop.updateChannel() ==> Poller.updateChannel();
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    void hasChannel(Channel *channel);

    void assertInLoopThread()
    {
        if (!isInLoopThread())
        {
            exit(1);
        }
    }

    // 判断 EventLoop 对象是否在自己的线程里面,
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

private:
    // wakeup,
    void handleRead();
    // 执行回调,
    void doPendingFunctors();

private:
    using ChannelList = std::vector<Channel *>;

    std::atomic_bool looping_; // 事件循环继续 loop,
    std::atomic_bool quit_;    // 标志退出 loop 循环,

    const pid_t threadId_; // 当前 loop 所在线程的Id,

    Timestamp pollReturnTime_;       // poller 返回发生事件的 Channels 的时间点,
    std::unique_ptr<Poller> poller_; // eventloop 管理的 Poller, 帮 loop 监听所有的 ChannelList 上发生的事件,

    // mainReactor 给 subReactor ...
    // libevent 采用的是 sockerpair 来完成  mainReactor 给 subReactor ,
    // muduo 采用的比较新, linux 内核比较新的, eventfd(unsigned int initval, int flags) 来完成 mainReactor 给 subReactor,
    int wakeupFd_;                           // 当 mianloop 获取一个新用户的channel, 通过轮询算法获取一个 subloop, 通过该成员变量 唤醒 subloop, 处理 channel,
    std::unique_ptr<Channel> wakeupChannel_; // 封装 wakeupFd_ 和 感兴趣的事件, 这样就把 wakeupFd_ 给到了 Channel,

    ChannelList activeChannels_;
    // Channel *currentActiveChannel_;

    std::atomic_bool callingPendingFunctors_;                            // 标识当前 loop 是否有需要执行的回调操作,
    std::mutex mutex_;                                                   // 互斥锁用来保护下面 vector<> 容器的线程安全操作, (pendingFunctors_),
    std::vector<Functor> pendingFunctors_ /* __attribute__((mutex_)) */; // 存储 loop 需要执行的所有的回调操作,
};
