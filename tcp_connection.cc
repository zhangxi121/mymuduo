#include <errno.h>
#include <functional>
#include <unistd.h>

#include "tcp_connection.h"

#include "channel.h"
#include "event_loop.h"
#include "logger.h"
#include "socket.h"
#include "sockets_ops.h"
#include "string.h"

void defaultConnectionCallback(const TcpConnectionPtr &conn)
{
}

void defaultMessageCallback(const TcpConnectionPtr &conn, Buffer *buf, Timestamp receiveTime)
{
    buf->retrieveAll();
}

static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (nullptr == loop)
    {
        LOG_FATAL("%s:%s:%d TcpConnection Loop is nullptr! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop, const std::string &nameArg, int sockfd,
                             const InetAddress &localAddr, const InetAddress &peerAddr)
    : loop_(CheckLoopNotNull(loop)),
      name_(nameArg),
      state_(kConnecting),
      reading_(true),
      socket_(new Socket(sockfd)),
      channel_(new Channel(loop, sockfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr),
      highWaterMark_(64 * 1024 * 1024)
{
    // 下面给 channel 设置相应的回调函数, Poller 给 channel 通知感兴趣的事件发生了, channel 会回调相应的操作函数,
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));
    LOG_INFO("TcpConnection::ctor[ %s ] at fd=%d \n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d, state=%s \n", name_.c_str(), channel_->fd(), stateToString());
    assert(state_ == kDisconnected);
}

void TcpConnection::send(const std::string &message)
{
    if (kConnected == state_)
    {
        if (loop_->isInLoopThread())
        {
            sendInLoop(message.c_str(), message.size());
        }
        else
        {
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, message.c_str(), message.size()));
        }
    }
}

void TcpConnection::shutdown()
{
    if (kConnected == state_)
    {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

void TcpConnection::connectEstablished()
{
    loop_->assertInLoopThread();
    assert(state_ == kConnecting);
    setState(kConnected);
    channel_->tie(this->shared_from_this());
    channel_->enableReading(); // 向 Poller 注册 channel 的 EPOLL_IN 事件,

    // 新连接建立, 执行回调,
    connectionCallback_(this->shared_from_this());
}

void TcpConnection::connectDestroyed()
{
    loop_->assertInLoopThread();
    if (state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll(); // 把 channel 的所有感兴趣的事件, 从 Poller 中 delete 掉,

        connectionCallback_(this->shared_from_this());
    }
    channel_->remove(); // 把 channel 从 Poller 中删除掉,
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    loop_->assertInLoopThread();
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0)
    {
        // 已建立连接的用户, 有可读事件发生, 调用用户传入的回调操作 onMessage(),
        messageCallback_(this->shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (0 == 0)
    {
        // 客户端断开连接,
        handleClose();
    }
    else
    {
        // 出错了,
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead error");
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    loop_->assertInLoopThread();
    if (channel_->isWriting())
    {
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        if (n > 0)
        {
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0)
            {
                // 发送完成了,
                channel_->disableWriting();
                if (writeCompleteCallback_)
                {
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, this->shared_from_this()));
                }
                if (kDisconnecting == state_)
                {
                    shutdownInLoop();
                }
            }
        }
        else
        {
            LOG_ERROR("TcpConnection::handleWrite");
        }
    }
    else
    {
        LOG_ERROR("TcpConnection fd = %d is down, no more writing !\n", channel_->fd());
    }
}

void TcpConnection::handleClose()
{
    loop_->assertInLoopThread();
    LOG_INFO("TcpConnection::handleClose fd = %d, state = %s", channel_->fd(), stateToString());
    assert(state_ == kConnected || state_ == kDisconnecting);
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(this->shared_from_this());
    connectionCallback_(connPtr); // 用户给的 ConnectionCallback 在连接成功和连接关闭都会执行到,
    if (closeCallback_)
    {
        // closeCallback 是 TcpServer 给 TcpConnection 的 TcpServer::removeConnection() 方法,
        closeCallback_(connPtr);
    }
}

void TcpConnection::handleError()
{
    int err = sockets_ops::getSocketError(channel_->fd());
    char t_errnobuf[512] = {0};
    LOG_ERROR("TcpConnection::handleError name : %s - SO_ERROR = %d, errString : %s \n",
              name_.c_str(), err, ::strerror_r(err, t_errnobuf, sizeof(t_errnobuf)));
}

void TcpConnection::sendInLoop(const void *data, size_t len)
{
    loop_->assertInLoopThread();
    ssize_t nwrote = 0;
    size_t remaining = len;  // 还没发送完的数据,
    bool faultError = false; // 是否产生错误,

    // 之前调用过该 connection 的 shotdown(), 不能再进行发送了,
    if (kDisconnected == state_)
    {
        LOG_ERROR("disconnected, give up writing");
        return;
    }

    // 表示 channel 第一次开始写数据, 而且缓冲区没有待发送数据,
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = ::write(channel_->fd(), data, len);
        if (nwrote >= 0)
        {
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_) // 一次性发送完,
            {
                // 既然在这里数据发送完了, 就不用再给 channel 设置写回调 EPOLLOUT 事件了,
                loop_->queueInLoop(std::bind(writeCompleteCallback_, this->shared_from_this()));
            }
        }
        else
        {
            // nwrote < 0
            nwrote = 0;
            if (EWOULDBLOCK != errno)
            {
                // EWOULDBLOCK 由于非阻塞没有数据, 正常的一个返回,
                LOG_ERROR("TcpConnection::sendInLoop error!");

                // SIGEPIPE 、 ECONNRESET 接收到对端 sockfd 的重置,
                if (EPIPE == errno || ECONNRESET == errno)
                {
                    faultError = true;
                }
            }
        }
    }

    assert(remaining <= len);
    /**
     * 走到这里说明上面的一次 write() 并没有把数据全部发送出去, 剩余的数据需要保存到缓冲区当中,
     * 然后给 Channel 注册 EPOLLOUT 事件,
     * Poller 是 EPOLL_LT 模式, 如果TCP发送缓冲区空余, Poller 会不断给上层上报相应的 fd 的 EPOLLOUT 事件的,
     * Poller 发现 TCP的发送缓冲区有空间, 会通知相应的 sock-channel, 调用相应的 handleWrite() 回调方法,
     * 也就是调用 TcpConnection::handleWrite() 方法把发送缓冲区中的数据全部发送完成,
     */
    if (!faultError && remaining > 0)
    {
        // 目前发送缓冲区剩余的待发送数据的长度,
        // 当第一次时候还没往 outputBuf_.append() 时候, outputBuf_.readableBytes() 为0,
        // 也就是 if( remaining >= highWaterMark_),
        size_t leftLen = outputBuffer_.readableBytes();
        if (leftLen + remaining >= highWaterMark_ &&
            leftLen < highWaterMark_ &&
            highWaterMarkCallback_)
        {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, this->shared_from_this(), leftLen + remaining));
        }
        outputBuffer_.append(static_cast<const char *>(data) + nwrote, remaining);
        if (!channel_->isWriting())
        {
            channel_->enableWriting(); // 这里一定要注册 channel 的写事件, 否则 Poller 不会给 channel 通知 EPOLL_OUT,
        }
    }
}

void TcpConnection::shutdownInLoop()
{
    loop_->assertInLoopThread();
    if (!channel_->isWriting()) // 说明 outputBuf 缓冲区的数据都已经发送完成,
    {
        socket_->shutdownWrite(); // 关闭写端, 触发 EPOLL_HUP 事件,
    }
}

const char *TcpConnection::stateToString() const
{
    switch (state_)
    {
    case kDisconnected:
        return "kDisconnected";
    case kConnecting:
        return "kConnecting";
    case kConnected:
        return "kConnected";
    case kDisconnecting:
        return "kDisconnecting";
    default:
        return "unknown state";
    }
}
