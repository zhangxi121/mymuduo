#pragma once

#include <functional>
#include <memory>

#include "noncopyable.h"
#include "timestamp.h"

class EventLoop;

/**
 * Channel 理解为通道, 封装了 sockfd 和其感兴趣的 event, 如 EPOLLIN  EPOLLOUT 事件,
 * 还绑定了 poller 返回的具体事件,
 *
 * EventLoop 就是 Demultiplex, 包含了 epoll (Poller) 和 epoll感兴趣的 fd 以及对应的f发生了的事件(Channel),
 * Channel 包含了 fd已经感兴趣的事件, 以及发生事件,
 * Poller 是 这些事件向 Poller注册,  发生的事件 Poller 给我 Channel进行通知,
 * Channel 得到发生了的事件,调用预置的相应的回调操作,
 *
 * 多核心中不可能一个线程作为 EventLoop, 跟CPU核心数量一样的 loop线程, 一个线程一个loop, "one loop per thread",
 * 一个线程一个 EventLoop  一个EventLoop有一个Poller  一个Poller可以监听很多的 Channel,
 * 每个 Channel属于一个EventLoop, 但是一个 EventLoop 可以有很多个Channel,
 *
 * 所以 Channel 关联 fd 和 events, 一个fd对应一个Channel, 多个fd对应多个Channel,
 *
 */
class Channel
{
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

public:
    Channel(EventLoop *loop, int fd);
    ~Channel();

public:
    // fd 得到 Poller 通知以后, 处理事件, 调用相应的回调方法,
    void handleEvent(Timestamp receiveTime);

    // 设置回调函数对象,
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    /**
     * 防止当 Channel 被手动 remove 掉, Channel 还在执行回调操作, 所以用弱智能指针监听 Channel 是否被 remove 掉,
     * tie() 什么时候被调用, 一个 TcpConnection ,新连接创建的时候, channel 的 tie_ 绑定一个 TcpConnection 对象,
     */
    void tie(const std::shared_ptr<void> &);

    int fd() const { return fd_; }
    int events() const { return events_; }                     // 返回 fd 感兴趣事件,
    void set_revents(int revt) { revents_ = revt; }            // Poller监听的事件,
    bool isNoneEvent() const { return events_ == kNoneEvent; } // 当前的 Channel 底层的 fd 到底有没注册事件,

    // 设置fd相应的事件状态,
    void enableReading()
    {
        events_ |= kReadEvent;
        update(); // update() 调用  epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &temp); 将 fd 感兴趣的事件添加到 epoll 中,
    }
    void disableReading()
    {
        events_ &= (~kReadEvent);
        update();
    }
    void enableWriting()
    {
        events_ |= kWriteEvent;
        update();
    }
    void disableWriting()
    {
        events_ &= (~kWriteEvent);
        update();
    }
    void disableAll()
    {
        events_ = kNoneEvent;
        update();
    }

    // 返回fd当前的事件状态,
    bool isWriting() const
    {
        return events_ & kWriteEvent;
    }

    bool isReading() const
    {
        return events_ & kReadEvent;
    }

    // for Poller
    int index() const { return index_; }
    void set_index(int idx) { index_ = idx; }

    /**
     * 一个线程一个 EventLoop  一个EventLoop有一个Poller  一个Poller可以监听很多的 Channel,
     * 每个 Channel属于一个EventLoop, 但是一个 EventLoop 可以有很多个Channel,
     *
     */
    EventLoop *ownerLoop() { return loop_; }

    /**
     * 在 Channel 所属的 EventLoop 中, 把当前的Channel删除掉,  ChannelList 中删除当前 Channel,
     *
     */
    void remove();

private:
    /**
     * 在使能当前 Channel 所表达fd的时间的时候, 向 Poller 去更新fd事件,
     * 当改变 Channel 所表示的 fd 的事件后, 需要update()在 Poller 里面更新fd相应的事件 epoll_ctr(),
     * EventLoop 包含 ChannelList 和一个 Poller, Channel想要向Poller注册fd事件是需要通过 eventloop_ 来注册的,
     *
     */
    void update();

    /**
     * 根据具体接收到事件来执行相应的回调操作,
     * 根据 Poller 通知的 Channel 发生的具体事件, 由Channel负责调用具体的回调操作,
     *
     */
    void handleEventWithGuard(Timestamp receiveTime);

private:
    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_;
    const int fd_; // Poller 监听的对象,
    int events_;   // 注册 fd 感兴趣的事件, EPOLLIN | EPOLLOUT,
    int revents_;  // Poller 返回的具体发生的事件, EPOLLIN | EPOLLOUT,
    int index_;    //

    std::weak_ptr<void> tie_;
    bool tied_;

    // 因为 Channel 通道里面能够获知 fd 最终发生的具体的事件 revents, 所以它负责调用调用具体事件的回调操作,
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};
