#pragma once

#include "poller.h"

class PollPoller : public Poller
{
public:
    PollPoller(EventLoop *loop);
    ~PollPoller() override;

public:
    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;

private:
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;

private:
    using PollFdList = std::vector<struct pollfd>;
    PollFdList pollfds_;
};
