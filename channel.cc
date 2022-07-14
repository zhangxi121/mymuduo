#include <ctype.h>
#include <sys/epoll.h>

#include "channel.h"
#include "event_loop.h"
#include "logger.h"


namespace
{
    const int kNew = -1;    // 一个 Channel 还没有添加到 Poller 中,
    const int kAdded = 1;   // 一个 Channel 已经添加到 epoll 中去了,
    const int kDeleted = 2; // 一个 Channel 刚从 poller 里面删除掉, 已删除状态,
}


const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(kNew), tied_(false)
{
}

Channel::~Channel()
{
    LOG_INFO("Channel::~Channel");
}

void Channel::handleEvent(Timestamp receiveTime)
{
    if (tied_)
    {
        std::shared_ptr<void> guard = tie_.lock();
        if (guard)
        {
            handleEventWithGuard(receiveTime);
        }
    }
    else
    {
        handleEventWithGuard(receiveTime);
    }
}

/**
 * tie() 什么时候被调用, 一个 TcpConnection ,新连接创建的时候, channel 的 tie_ 绑定一个 TcpConnection 对象, 
 * 
 */
void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;
    tied_ = true;
}

/**
 * 在使能当前 Channel 所表达fd的时间的时候, 向 Poller 去更新fd事件,
 * 当改变 Channel 所表示的 fd 的事件后, 需要update()在 Poller 里面更新fd相应的事件 epoll_ctr(),
 * EventLoop 包含 ChannelList 和一个 Poller, Channel想要向Poller注册fd事件是需要通过 eventloop_ 来注册的,
 *
 */
void Channel::update()
{
    // 通过 Channel 所属的 EventLoop 调用 Poller 相应方法, 来注册fd的events,
    loop_->updateChannel(this);
}

void Channel::remove()
{
    loop_->removeChannel(this);
}


// 根据 Poller 通知的 Channel 发生的具体事件, 由Channel负责调用具体的回调操作,
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO("Channel handleEvent revents:%d\n", revents_);
    
    // 出问题了, 发生异常了,
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if (closeCallback_)
        {
            closeCallback_();
        }
    }

    // EPOLLERR,
    if (revents_ & EPOLLERR)
    {
        if (errorCallback_)
            errorCallback_();
    }

    // EPOLLIN 可读事件,
    if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP))
    {
        if (readCallback_)
            readCallback_(receiveTime);
    }

    // EPOLLOUT 可写事件,
    if (revents_ & EPOLLOUT)
    {
        if (writeCallback_)
            writeCallback_();
    }
}
