#include <netinet/in.h>
#include <netinet/tcp.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "socket.h"

#include "inet_address.h"
#include "logger.h"
#include "sockets_ops.h"

Socket::~Socket()
{
    ::close(sockfd_);
}

void Socket::bindAddress(const InetAddress &localAddr)
{
    if (0 != ::bind(sockfd_, (const struct sockaddr *)localAddr.getSockAddr(), sizeof(struct sockaddr_in)))
    {
        LOG_FATAL("bind sockfd:%d fail !\n", sockfd_);
    }
}

void Socket::listen()
{
    if (0 != ::listen(sockfd_, 1024))
    {
        LOG_FATAL("listen sockfd:%d fail !\n", sockfd_);
    }
}

int Socket::accept(InetAddress *peerAddr)
{
    /**
     * 1. accept() 函数的参数不合法,
     *     The addrlen argument is a value-result argument: the caller must initialize it  to  contain  the  size  (in
     *     bytes) of the structure pointed to by addr; on return it will contain the actual size of the peer address.
     *     参数 addrlen 必须初始化,
     *
     * 2. 对返回的 connfd 没有设置非阻塞,
     *     Reactor模型 one loop per thread,
     *     poll + non-blocking IO,
     *
     */

    struct sockaddr_in cliAddr;
    socklen_t len = sizeof(struct sockaddr_in);
    bzero(&cliAddr, sizeof(cliAddr));

#if defined(NO_ACCEPT4)
    int connfd = ::accept(sockfd_, (sockaddr *)&cliAddr, &len);
    sockets_ops::setNonBlockAndCloseOnExec(connfd);
#else
    int connfd = ::accept4(sockfd_, (sockaddr *)&cliAddr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
#endif

    if (connfd >= 0)
    {
        peerAddr->setSockAddrInet(cliAddr);
    }

    return connfd;
}

void Socket::shutdownWrite()
{
    if (::shutdown(sockfd_, SHUT_WR) < 0)
    {
        LOG_ERROR("shutdownWrite error! \n");
    }
}

void Socket::setTcpNoDelay(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

void Socket::setReuseAddr(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

void Socket::setReusePort(bool on)
{
    int optval = on ? 1 : 0;
    int ret = ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
}

void Socket::setKeepAlive(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}
