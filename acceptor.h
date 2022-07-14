#pragma once

#include <functional>

#include "channel.h"
#include "socket.h"

class EventLoop;
class InetAddress;

class Acceptor : noncopyable
{
public:
    /**
     * 在 void(int sockfd, const InetAddress &) 回调中, 轮询找到 subLoop, 唤醒 并 分发当前的新客户端的 Channel,
     */
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress &)>;

public:
    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();

public:
    void setNewConnectionCallback(const NewConnectionCallback &cb) { newConnectionCallback_ = cb; }
    bool listenning() const { return listenning_; }
    void listen();

private:
    /**
     * 回调函数, 当 listenFd 有事件发生了, 用新用户连接了,
     */
    void handleRead(Timestamp receiveTime);
    // void handleRead();


private:
    EventLoop *loop_; // Acceptor 用户就是用户定义的 baseLoop, 也称作 mainLoop,
    Socket acceptSocket_;
    Channel acceptChannel_;

    /**
     * 一个客户端连接成功后, TcpServer 通过轮询选择一个 subLoop 并唤醒,
     * 并把 mainLoop 拿到的 connfd 打包成一个 Channel, 扔给 subLoop,
     * NewConnectionCallback() 就负责处理这些事情, 打包 fd 成 Channel,
     * getNextLoop() 唤醒一个 subLoop, 再把这个 Channel 分发给相应的 loop, 去监听已连接用户的读写事件,
     */
    NewConnectionCallback newConnectionCallback_;
    bool listenning_;
};
