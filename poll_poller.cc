#include <assert.h>
#include <poll.h>

#include "poll_poller.h"

#include "channel.h"
#include "logger.h"

PollPoller::PollPoller(EventLoop *loop) : Poller(loop) {}
PollPoller::~PollPoller() {}

Timestamp PollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    int numEvents = ::poll(&*pollfds_.begin(), pollfds_.size(), timeoutMs);
    int savedErrno = errno;
    Timestamp now(Timestamp::now());
    if (numEvents > 0)
    {
        LOG_INFO("events happened \n");
        fillActiveChannels(numEvents, activeChannels);
    }
    else if (0 == numEvents)
    {
        // 没有 fd 发生事件, 只是 timeout 超时了,
        LOG_DEBUG("%s poll timeout! \n", __FUNCTION__);
    }
    else
    {
        // error happens, log uncommon ones
        // EINTR 外部的中断,
        if (EINTR != savedErrno)
        {
            errno = savedErrno;
            LOG_ERROR("PollPoller::poll() : errno = EINTR\n");
        }
    }

    return now;
}

void PollPoller::updateChannel(Channel *channel)
{
    LOG_INFO("func=%s => fd=%d, events=%d, index=%d \n", __FUNCTION__, channel->fd(), channel->events(), channel->index());

    if (channel->index() < 0)
    {
        struct pollfd pfd;
        pfd.fd = channel->fd();
        pfd.events = static_cast<short>(channel->events());
        pfd.revents = 0;
        pollfds_.push_back(pfd);
        int idx = static_cast<int>(pollfds_.size()) - 1;
        channel->set_index(idx);
        channels_[pfd.fd] = channel;
    }
    else
    {
        if ((channels_.find(channel->fd()) != channels_.end()) &&
            (channels_[channel->fd()] == channel))
        {
            int idx = channel->index();
            if (0 <= idx && idx < static_cast<int>(pollfds_.size()))
            {
                struct pollfd &pfd = pollfds_[idx];
                if (pfd.fd == channel->fd() || pfd.fd == -channel->fd() - 1)
                {
                    pfd.fd = channel->fd();
                    pfd.events = static_cast<short>(channel->events());
                    pfd.revents = 0;

                    if (channel->isNoneEvent())
                    {
                        // ignore this pollfd
                        pfd.fd = -channel->fd() - 1;
                    }
                }
            }
        }
    }
}

void PollPoller::removeChannel(Channel *channel)
{

    if ((channels_.find(channel->fd()) != channels_.end()) &&
        channels_[channel->fd()] == channel &&
        channel->isNoneEvent())
    {
        int idx = channel->index();
        assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
        const struct pollfd &pfd = pollfds_[idx];
        LOG_INFO("func=%s => pfd.fd=%d \n", __FUNCTION__, pfd.fd);
        (void)pfd;

        assert(pfd.fd == -channel->fd() - 1 && pfd.events == channel->events());
        size_t n = channels_.erase(channel->fd());
        assert(n == 1);
        (void)n;
        if (idx == pollfds_.size() - 1)
        {
            pollfds_.pop_back();
        }
        else
        {
            int channelAtEnd = pollfds_.back().fd;
            iter_swap(pollfds_.begin() + idx, pollfds_.end() - 1);
            if (channelAtEnd < 0)
            {
                channelAtEnd = -channelAtEnd - 1;
            }
            channels_[channelAtEnd]->set_index(idx);
            pollfds_.pop_back();
        }
    }
}

void PollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for (PollFdList::const_iterator pfd = pollfds_.begin(); pfd != pollfds_.end() && numEvents > 0; ++pfd)
    {
        if (pfd->revents > 0)
        {
            --numEvents;
            ChannelMap::const_iterator ch = channels_.find(pfd->fd);
            if (ch != channels_.end())
            {
                Channel *channel = ch->second;
                assert(channel->fd() == pfd->fd);
                channel->set_revents(pfd->revents);
                // pfd->revents = 0;
                activeChannels->push_back(channel);
            }
        }
    }
}
