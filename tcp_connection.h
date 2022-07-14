#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "buffer.h"
#include "callbacks.h"
#include "inet_address.h"
#include "noncopyable.h"
#include "timestamp.h"

class Channel;
class EventLoop;
class Socket;

/**
 * 客户端和服务器已经建立连接, 打包成功连接服务器的客户端的通信链路的,
 * TcpServer ==> Acceptor ==> 有一个新用户连接, 通过 accept() 函数拿到 connfd ==> 打包 TcpConnection, 设置相应的回调,
 * 回调: TcpServer.setConnectionCallback()  ==> TcpConnection.setConnectionCallback() ==>
 *       ==> channel_.setConnectionCallback() ==> Poller ==> channel 的回调操作 Channel::handleEvent(),
 *
 */
class TcpConnection : noncopyable,
                      public std::enable_shared_from_this<TcpConnection>
{
public:
    /**
     * loop 是外传传来的, tcpServer::newConnection() 中遍历拿到一个 ioLoop, 把这个 ioLoop 给进来,
     *
     */
    TcpConnection(EventLoop *loop, const std::string &nameArg, int sockfd,
                  const InetAddress &localAddr, const InetAddress &peerAddr);
    ~TcpConnection();

public:
    EventLoop *getLoop() const { return loop_; }
    const std::string &name() const { return name_; }
    const InetAddress &localAddress() const { return localAddr_; }
    const InetAddress &peerAddress() const { return peerAddr_; }
    bool connected() const { return state_ == kConnected; }
    bool disconnected() const { return state_ == kDisconnected; }

    void send(const std::string &message);

    /**
     * 关闭写端, TcpConnection::shutdownInLoop() ==> Socket::shutdownWrite() ==> ::shutdown(sockfd_, SHUT_WR) ==>
     * ::shutdown()关闭写端 ==> 触发 EPOLL_HUP 事件 ==>  channel_->closeCallback_() ==> TcpServer::removeConnection() ==>
     * 最终调用到 TcpServer::removeConnection(), 删除连接 TcpConnection::connectDestroyed(),
     */
    void shutdown();

    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }

    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }

    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }

    void setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark)
    {
        highWaterMarkCallback_ = cb;
        highWaterMark_ = highWaterMark;
    }

    void setCloseCallback(const CloseCallback &cb) { closeCallback_ = cb; }

    // 连接建立了,
    void connectEstablished();

    // 连接销毁,
    void connectDestroyed();

private:
    enum StateE
    {
        kDisconnected,
        kConnecting,
        kConnected,
        kDisconnecting
    };

private:
    /**
     * 在构造函数中设置 channel_ 的回调,
     * TcpConnection::TcpConnection() {
     *    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
     *    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
     *    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
     *    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));
     * }
     * 在这些回调中完成 读、写、关闭连接 等事情,
     * Poller ==> channel_->readCallback_() ==> this->handleRead(),
     * Poller ==> channel_->writeCallback_() ==> this->handleWrite(),
     * Poller ==> channel_->closeCallback_() ==> this->handleClose(),
     * Poller ==> channel_->errorCallback_() ==> this->errorCallback_(),
     *
     */
    // Poller ==> channel_->readCallback_() ==> this->handleRead(),
    void handleRead(Timestamp receiveTime);
    // Poller ==> channel_->writeCallback_() ==> this->handleWrite(),
    void handleWrite();

    /**
     * Poller ==> channel_->closeCallback_() ==> this->handleClose() ==> 
     *   ==> TcpServer::removeConnection() ==> TcpConnection::connectDestroyed(),
     *   所以最终还是调用到了 this->connectDestroyed() 里面了,
     */   
    void handleClose();
    
    // Poller ==> channel_->errorCallback_() ==> this->errorCallback_(),
    void handleError();

    /**
     * 发送数据, 应用写得快, 而且内核发送数据慢, 我们需要待发送数据写入缓冲区,
     * 而且设置了水位回调 HighWaterMarkCallback, 防止发送太快,
     */
    void sendInLoop(const void *data, size_t len);
    void shutdownInLoop();

    void setState(StateE s) { state_ = s; }
    const char *stateToString() const;

private:
    EventLoop *loop_; // 这里是 subLoop, 因为 TCPConnection 都是在 subLoop 管理的,
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    // 这里和 Acceptor 类似, Acceptor 是在 mainLoop 里面的, 而 TcpConnection 是在 subLoop 里面的,
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;
    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    size_t highWaterMark_;

    Buffer inputBuffer_;  // 接收数据的缓冲区,
    Buffer outputBuffer_; // 发送数据的缓冲区,
};
