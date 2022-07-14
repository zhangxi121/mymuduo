#pragma once

#include "noncopyable.h"

class InetAddress;

class Socket : noncopyable
{
public:
    explicit Socket(int sockfd) : sockfd_(sockfd) {}
    ~Socket();

public:
    int fd() const { return sockfd_; }
    void bindAddress(const InetAddress &localAddr);
    void listen();
    int accept(InetAddress *peerAddr);
    void shutdownWrite();

    /**
     * Enable/disable TCP_NODELAY (disable/enable Nagle's algorithm).
     * 直接发送, 对于数据不进行 Tcp 缓冲,
     */
    void setTcpNoDelay(bool on);
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setKeepAlive(bool on);

public:
private:
    const int sockfd_;
};
