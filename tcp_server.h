#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include "callbacks.h"
#include "noncopyable.h"

// class Acceptor;
// class EventLoop;
// class EventLoopThreadpool;
#include "acceptor.h"
#include "event_loop.h"
#include "event_loop_threadpool.h"
#include "tcp_connection.h"

/**
 * 用户使用 muduo 编写服务器程序,
 *
 */

class TcpServer : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;
    enum Option
    {
        kNoReusePort,
        kReusePort,
    };

public:
    TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string &nameArg, Option option = kNoReusePort);
    ~TcpServer();

public:
    const std::string &ipPort() const { return ipPort_; }
    const std::string name() const { return name_; }
    EventLoop *getLoop() const { return loop_; }

    void setThreadInitCallback(const ThreadInitCallback &cb) { threadInitCallback_ = cb; }
    // 设置底层 subLoop 的个数, Thread  EventLoopThread  EventLoopThreadpool 都是通过这个函数触发的,
    void setThreadNum(int numThreads);

    /**
     * 开始服务器监听, 实际上就是开启 mainLoop 的 Acceptor.listen(),
     * tcpServer.start();  loop->loop();
     */
    void start();

    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }

private:
    /**
     * 处理新用户的连接, 根据轮询算法, 选择一个 subLoop, 唤醒 subLoop,
     * 把当前 connfd 封装成 Channel, 分发给 subLoop,
     * Channel 封装的回调是当有消息来了以后, 消息处理的回调,
     * ...
     * acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection,xx)) ==>
     * TcpServer::start() ==>
     * Acceptor::handleRead() { newConnectionCallback_(connfd, peerAddr); }  ==> TcpServer::newConnection()  { ... }
     * 有一个新的客户端的连接会执行  TcpServer::newConnection() 回调操作,
     * 参数 sockfd  peerAddr 是 Acceptor::handleRead() 给传进来的,
     */
    void newConnection(int sockfd, const InetAddress &peerAddr);
    // 从 ConnectionMap 里面移除,
    void removeConnection(const TcpConnectionPtr &conn);
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

private:
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

private:
    EventLoop *loop_; // 用户定义的 baseLoop_;
    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_;              // 运行在 mianLoop 的 Accrptor, 监听新连接事件,
    std::shared_ptr<EventLoopThreadpool> threadPool_; // one loop per thread,

    ConnectionCallback connectionCallback_;       // 有新连接时的回调,
    MessageCallback messageCallback_;             // 有读写消息时的回调,
    WriteCompleteCallback writeCompleteCallback_; // 消息发送完成以后的回调,
    ThreadInitCallback threadInitCallback_;       // loop_ 线程初始化的回调,

    std::atomic_int started_; // 防止一个 tcpServer 对象被 start 多次,
    int nextConnId_;
    ConnectionMap connections_; // 保存所有的连接,
};
