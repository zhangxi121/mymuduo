#pragma once

#include <unordered_map>
#include <vector>

#include "noncopyable.h"
#include "timestamp.h"

class Channel;
class EventLoop;

/**
 * muduo库中的 Demultiplex 的核心 IO复用模块,  负责事件的监听 、 开启事件循环
 *
 */
class Poller : noncopyable
{
public:
    using ChannelList = std::vector<Channel *>;

public:
    Poller(EventLoop *loop);
    virtual ~Poller() = default;

public:
    // 给所有IO保留统一的接口,
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;

    virtual void updateChannel(Channel *channel) = 0;

    virtual void removeChannel(Channel *channel) = 0;

    // 判断参数 Channel 是否在当前 Poller 当中,
    virtual bool hasChannel(Channel *channel) const;

    /**
     * EventLoop 事件循环, 可以通过该接口获取默认的Io复用的具体实现的对象, 类似于 getInstance(),
     * #include "PollPoller.h"     #include "EPollPoller.h"
     * 这里留给具体的派生类去实现,
     */ 
    static Poller *newDefaultPoller(EventLoop *loop);

protected:
    // unordered_map<> 的key 表示 sockfd, value 表示 sockfd 所属的 Channel,
    using ChannelMap = std::unordered_map<int, Channel *>;
    ChannelMap channels_;

private:
    EventLoop *ownerLoop_; // 定义 Poller 所属的事件循环 EventLoop,
};
