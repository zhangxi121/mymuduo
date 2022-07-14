#include <assert.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "epoll_poller.h"

#include "channel.h"
#include "logger.h"

namespace
{
    const int kNew = -1;    // 一个 Channel 还没有添加到 Poller中,
    const int kAdded = 1;   // 一个 Channel 已经添加到 epoll 中去了,
    const int kDeleted = 2; // 一个 Channel 刚从 poller 里面删除掉, 已删除状态,

    static void testEpollEvents(uint32_t events)
    {
        if ((events & EPOLLHUP) && !(events & EPOLLIN))
            std::cout << "============== EPOLLHUP " << std::endl;
        if (events & EPOLLERR)
            std::cout << "============== EPOLLERR " << std::endl;
        if (events & (EPOLLIN | EPOLLPRI | EPOLLRDHUP))
            std::cout << "============== EPOLLIN " << std::endl;
        if (events & EPOLLOUT)
            std::cout << "============== EPOLLOUT " << std::endl;
    }

}

EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop), epollfd_(::epoll_create1(EPOLL_CLOEXEC)), events_(kInitEventListSize)
{
    if (epollfd_ < 0)
    {
        LOG_FATAL("epoll_create() error:%d\n", errno);
    }
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

/**
 *  while (1)
    {
        // 委托内核做IO检测，epoll ...

        ret = epoll_wait(epfd, all, sizeof(all) / sizeof(all[0]), -1);
        printf("================= epoll_wait ================\n");

        for (int i = 0; i < ret; i++)
        {
        }
    }
 *
 * epoll_wait(), 所以这个函数会非常频繁, LOG_DEBUG()  LOG_ERROR() 更合适,
 *
 */
Timestamp EPollPoller::poll(int timeoutMs, ChannelList *_out_activeChannels)
{
    LOG_INFO("func=%s => fd_count = %lu \n", __FUNCTION__, channels_.size());

    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    // 之前在 EPollPoller::update() 设置了
    // 因为 poll() 在多个线程中调用, 所以这里要保存起来全局的 errno,
    int savedErrno = errno;
    Timestamp now(Timestamp::now());

    if (numEvents > 0)
    {
        // 有已经发生了的事件,
        LOG_INFO("%d events happened \n", numEvents);

        {
#if 0
            for (int i = 0; i < numEvents; ++i)
            {
                int fd = events_[i].data.fd;
                Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
                uint32_t revents = events_[i].events;
                printf("fd = %d, channel = %p\n", fd, channel);
                testEpollEvents(revents);
            }
#endif
        }

        fillActiveChannels(numEvents, _out_activeChannels);
        if (numEvents == events_.size())
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if (0 == numEvents)
    {
        // 没有 fd 发生事件, 只是 timeout 超时了,
        LOG_DEBUG("%s epoll_wait timeout! \n", __FUNCTION__);
    }
    else
    {
        // error happens, log uncommon ones
        // EINTR 外部的中断,
        if (EINTR != savedErrno)
        {
            errno = savedErrno;
            LOG_ERROR("EPollPoller::poll() : errno = EINTR\n");
        }
    }

    return now;
}

/**
 * epoll_ctl(), channel.updateChannel() --> loop->updateChannel() --> poller->updateChannel(),
 * Eventloop 包含了 ChannelList 和 Pooler,
 * Pooler 包含了 ChannelMap,
 * ChannelMap 包含了 unorderer_map<fd, Channel*>
 *
 */

void EPollPoller::updateChannel(Channel *channel)
{
    const int index = channel->index();
    LOG_INFO("func=%s => fd=%d, events=%d, index=%d \n", __FUNCTION__, channel->fd(), channel->events(), channel->index());
    if (kNew == index || kDeleted == index)
    {
        if (kNew == index)
        {
            // 从来没有添加到 Poller 中,
            int fd = channel->fd();
            channels_[fd] = channel;
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }
    else
    {
        // update existing one with EPOLL_CTL_MOD/DEL
        // Channel 已经在 Poller上注册过了, 需要修改为 OLL_CTL_MOD/DEL,
        int fd = channel->fd();
        (void)fd;
        if (channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

// 从 Poller 中删除 Channel,
void EPollPoller::removeChannel(Channel *channel)
{
    int fd = channel->fd();
    if ((channels_.find(fd) != channels_.end()) &&
        channels_[fd] == channel &&
        channel->isNoneEvent())
    {
        channels_.erase(fd);
    }

    LOG_INFO("func=%s => fd=%d \n", __FUNCTION__, channel->fd());

    int index = channel->index();
    if (kAdded == index)
    {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

//
void EPollPoller::fillActiveChannels(int numEvents, ChannelList *_out_activeChannels) const
{
    for (int i = 0; i < numEvents; ++i)
    {
        Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        _out_activeChannels->push_back(channel); // EventLoop 就拿到了 Poller 给它返回的所有发生事件的 Channel 列表, 就能处理 读事件 写事件,
    }
}

// epoll_ctl() EPOLL_CTL_ADD   EPOLL_CTL_DEL   EPOLL_CTL_MOD 具体的操作,
void EPollPoller::update(int operation, Channel *channel)
{
    struct epoll_event event;
    // memset(&event, 0, sizeof(event));
    bzero(&event, sizeof(event));
    int fd = channel->fd();
    event.events = channel->events();
    event.data.ptr = channel; // 调试崩溃, 定位到 channel* 地址问题,
    // event.data 是一个 union 联合体,  所以这里设置了  event.data.ptr = channel; 之后, 
    // 就不要再设置 event.data.fd = fd; 否则非法地址访问段错误,

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (EPOLL_CTL_DEL == operation)
        {
            LOG_ERROR("EPOLL_CTL_DEL error:%d \n", errno);
        }
        else
        {
            LOG_FATAL("EPOLL_CTL_ADD  EPOLL_CTL_MOD error:%d\n", errno);
        }
    }
}
