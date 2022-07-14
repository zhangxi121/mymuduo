#include "tcp_server.h"

#include "acceptor.h"
#include "event_loop.h"
#include "event_loop_threadpool.h"
#include "logger.h"
#include "sockets_ops.h"
#include "tcp_connection.h"

/**
 * TcpServer 创建一个 tcpServer 对象, 初始化时候创建 acceptor_ 对象,
 * acceptor_ 构造函数中创建 socket, 封装 Channel, 同时构造函数中  acceptChannel_.setReadCallback(ReadEventCallback cb) 绑定回调,
 * 其中 ReadEventCallback 绑定的是 Acceptor::handleRead, 也就是当 acceptChannel_.fd 有事件发生的话会调用到 Acceptor::handleRead()函数,
 * 在 acceptor_.listen() 中 通过 acceptChannel_.enableReading() 添加监听读事件,
 * 当有读事件来了以后, 就会跑到 Acceptor::handleRead()  中去执行业务,
 *
 * tcpServer.start() ==> acceptor_.listen() ==>  acceptChannel_.enableReading(), 添加监听读事件,
 * 有事件发生 ReadEventCallback 就会回调到 Acceptor::handleRead() 里面去,
 * Acceptor::handleRead() { int connfd = acceptSocket_.accept(&peerAddr);  newConnectionCallback_(connfd, peerAddr); }
 * 其中 newConnectionCallback_ 是通过 TcpServer::TcpServer() { acceptor_->setNewConnectionCallback(); } 设置 newConnectionCallback_ 回调的,
 *
 */

static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (nullptr == loop)
    {
        LOG_FATAL("%s:%s:%d mainLoop is nullptr! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

/**
 * TcpServer::TcpServer() {}
 * 1. 创建 acceptor_()
 * 2. acceptor_ 构造中完成 acceptChannel_ 构造, 将 acceptorFd 给到 acceptChannel_,
 * 3. acceptor_ 构造中 acceptChannel_.setReadCallback(ReadEventCallback cb) 绑定回调, 其中 ReadEventCallback 绑定的是 Acceptor::handleRead,
 * 4. acceptor_->setNewConnectionCallback() 设置 Acceptor.newConnectionCallback_ 回调,
 *
 * TcpServer::start() {}
 * 1. tcpServer.start() ==> acceptor_.listen() ==>  acceptChannel_.enableReading(), 添加监听读事件,
 * 2. 有事件发生 ReadEventCallback 就会回调到 Acceptor::handleRead() 里面去,
 * 3. Acceptor::handleRead() { int connfd = acceptSocket_.accept(&peerAddr);  newConnectionCallback_(connfd, peerAddr); }
 *
 */
TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string &nameArg, Option option)
    : loop_(CheckLoopNotNull(loop)),
      ipPort_(listenAddr.toIpPort()),
      name_(nameArg),
      acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),
      threadPool_(new EventLoopThreadpool(loop, name_)),
      connectionCallback_(defaultConnectionCallback),
      messageCallback_(defaultMessageCallback),
      started_(0),
      nextConnId_(1)
{
    // 当有新用户连接时, 会执行 TcpServer::newConnection() 回调,
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this,
                                                  std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{
    loop_->assertInLoopThread();
    LOG_INFO("TcpServer::~TcpServer [%s] destructing", name_.c_str());

    for (std::pair<const std::string, TcpConnectionPtr> &item : connections_)
    {
        TcpConnectionPtr conn(item.second);
        item.second.reset(); // item.second 不再使用强智能指针去管理资源, item.second 资源就可以释放,
                             // 因为强智能指针引用的资源是无法释放掉的, 这样就只有 conn 管理这个 TcpConnection 对象,
                             // 当出了作用域, 就自动释放 TcpConnection 对象,
        conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

void TcpServer::setThreadNum(int numThreads)
{
    assert(0 <= numThreads);
    threadPool_->setThreadNum(numThreads);
}

// tcpServer.start();  loop->loop();
void TcpServer::start()
{
    // 防止一个 tcpServer 对象被 start 多次,
    if (started_++ == 0)
    {
        threadPool_->start(threadInitCallback_);
        assert(!acceptor_->listenning());
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
        // 因为 loop 是主loop, 那么直接在主线程里面, 直接就执行了 Acceptor::listen() 函数了,
    }
}

/**
 * acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection,xx)) ==>
 * TcpServer::start() ==>
 * Acceptor::handleRead() { newConnectionCallback_(connfd, peerAddr); }  ==> TcpServer::newConnection()  { ... }
 * 有一个新的客户端的连接会执行  TcpServer::newConnection() 回调操作,
 * 参数 sockfd  peerAddr 是 Acceptor::handleRead() 给传进来的,
 */
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    loop_->assertInLoopThread();
    // 轮询算法选择一个 subLoop 来管理 Channel,
    EventLoop *ioLoop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s\n",
             name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str())

    // sockfd 是 connect() 之后返回的, 通过 sockfd 获取其本机绑定的ip和port,
    struct sockaddr_in local = sockets_ops::getLocalAddr(sockfd);
    InetAddress localAddr(local);
    // 根据连接成功的 sockfd, 创建 TcpConnection 连接对象,
    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
    connections_[connName] = conn;
    // 西面的回调, 用户设置给TcpServer ==>  TcpConnection ==> Channel ==> Poller,
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    // 设置了如何关闭连接的回调,
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    // 直接让 ioLoop 直接调用 TcpConnection::connectEstablished()
    // ==> conn.channel_->enableReading();  conn.connectionCallback_();[用户预置的 onConnection_ 就会回调]
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    loop_->assertInLoopThread();
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n", name_.c_str(), conn->name().c_str());
    size_t n = connections_.erase(conn->name()); // 返回的是删除的个数,
    assert(n == 1);
    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}
