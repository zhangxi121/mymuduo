#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "acceptor.h"

#include "event_loop.h"
#include "inet_address.h"
#include "logger.h"

static void setNonBlockAndCloseOnExec(int sockfd)
{
    int flags = ::fcntl(sockfd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    int ret = ::fcntl(sockfd, F_SETFL, flags);

    flags = ::fcntl(sockfd, F_GETFD, 0);
    flags |= FD_CLOEXEC;
    ret = ::fcntl(sockfd, F_SETFD, flags);
}

static int createNonblocking()
{
    sa_family_t family = AF_INET;
    int sockfd = ::socket(family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd < 0)
    {
        LOG_FATAL("%s:%s:%d createNonblocking error, errno:%d!", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop),
      acceptSocket_(createNonblocking()),
      acceptChannel_(loop, acceptSocket_.fd()),
      listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);

    // TcpServer.start() 会启动 Accepter.listen(),
    // Accept 运行起来后, 有新用户的连接, 要执行一个回调操作, 在这个毁掉里面把 connfd 打包成 Channel,
    // 唤醒一个 subLoop, 把 Channel 给 subLoop, 由 subLoop 去处理客户端的读写事件,
    // baseLoop 监听到 acceptChannel 有事件发生的话,,
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this, std::placeholders::_1));
    // acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
    // 可读事件,
}

Acceptor::~Acceptor()
{
    LOG_INFO("Acceptor::~Acceptor");
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen()
{
    if (!loop_->isInLoopThread())
    {
        abort();
    }
    listenning_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();
}

// void Acceptor::handleRead(void) {}


// void Acceptor::handleRead()
void Acceptor::handleRead(Timestamp receiveTime)
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >= 0)
    {
        if (newConnectionCallback_)
        {
            newConnectionCallback_(connfd, peerAddr);
        }
        else
        {
            ::close(connfd);
        }
    }
    else
    {
        // $ perror 22  #==> OS error code  22:  Invalid argument

        LOG_ERROR("%s:%s:%d Acceptor::handleRead error, errno:%d!", __FILE__, __FUNCTION__, __LINE__, errno);
        if (EMFILE == errno)
        {
            LOG_ERROR("%s:%s:%d Acceptor::handleRead sockfd reached limit!", __FILE__, __FUNCTION__, __LINE__);
        }
    }
}