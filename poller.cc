#include "poller.h"

#include "channel.h"

Poller::Poller(EventLoop *loop) : ownerLoop_(loop) {}

bool Poller::hasChannel(Channel *channel) const
{
    ChannelMap::const_iterator cit = channels_.find(channel->fd());
    return cit != channels_.end() && cit->second == channel;
}
