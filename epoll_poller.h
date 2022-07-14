#pragma once

#include <sys/epoll.h>
#include <vector>

#include "poller.h"

/**
 * epoll的使用
 * epoll_create()
 * epoll_ctl()  ADD/MODIFY/DEL
 * epoll_wait()
 *
 */

class EPollPoller : public Poller
{
public:
    // epoll_create(),
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override;

public:
    // epoll_wait(),
    Timestamp poll(int timeoutMs, ChannelList *_out_activeChannels) override;

    // epoll_ctl(), channel.updateChannel() --> loop->updateChannel() --> poller->updateChannel(),
    void updateChannel(Channel *channel) override;
    // epoll_ctl(), channel.removeChannel() --> loop->removeChannel() --> poller->removeChannel(),
    void removeChannel(Channel *channel) override;

private:
    // 填写活跃的连接,
    void fillActiveChannels(int numEvents, ChannelList *_out_activeChannels) const;
    // 更新 Channel 通道, 被上面的 update() 调用,
    void update(int operation, Channel *channel);

private:
    static const int kInitEventListSize = 16;

    // 发生事件的 epoll_event 数组,
    using EventList = std::vector<struct epoll_event>;
    int epollfd_;
    EventList events_;
};
