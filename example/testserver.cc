#include <functional>
#include <string>
#include <unistd.h>


#include <mymuduo/logger.h>
#include <mymuduo/tcp_server.h>

class EchoServer
{
public:
    EchoServer(EventLoop *loop, const InetAddress &localAddr, const std::string &name)
        : server_(loop, localAddr, name), loop_(loop)
    {
        // 注册回调函数,
        server_.setConnectionCallback(std::bind(&EchoServer::onConnection, this, std::placeholders::_1));
        server_.setMessageCallback(std::bind(&EchoServer::onMessage, this,
                                             std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        // 设置合适的 loop 线程数量, subLoop,
        server_.setThreadNum(3);
    }
    ~EchoServer() {}

    void start()
    {
        server_.start();
    }

private:
    // 连接建立或者断开的回调,
    void onConnection(const TcpConnectionPtr &conn)
    {
        if (conn.get()->connected())
        {
            LOG_INFO("new connection UP : %s", conn->peerAddress().toIpPort().c_str());
        }
        else
        {
            LOG_INFO("new connection DOWN : %s", conn->peerAddress().toIpPort().c_str());
        }
    }

    // 可读写事件回调,
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time)
    {
        std::string msg = buf->retrieveAllAsString();
        conn->send(msg);
        conn->shutdown();
    }

private:
    EventLoop *loop_;
    TcpServer server_;
};

int main(int argc, char const *argv[])
{
    EventLoop loop;
    InetAddress addr(8000, "0.0.0.0");
    EchoServer server(&loop, addr, "EchoServer01");
    server.start(); // listen loopthread listen ==> Acceptor ==> mainLoop ==> 等待连接, 有新用户连接就回调到 TcpServer::newConnection(),
    loop.loop();    // 启动 mainLoop 的底层 Poller,

    return 0;
}
