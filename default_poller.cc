
#include <stdlib.h>

#include "epoll_poller.h"
#include "poll_poller.h"
#include "poller.h"

Poller *Poller::newDefaultPoller(EventLoop *loop)
{
    if (::getenv("MUDUO_USE_POLL"))
    {
        return new PollPoller(loop);
    }
    else
    {
        return new EPollPoller(loop);
    }
}