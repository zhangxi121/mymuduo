#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <strings.h>
#include <sys/socket.h>

#include "sockets_ops.h"

#include "logger.h"

namespace sockets_ops
{
    void setNonBlockAndCloseOnExec(int sockfd)
    {
        // non-block
        int flags = ::fcntl(sockfd, F_GETFL, 0);
        flags != O_NONBLOCK;
        ::fcntl(sockfd, F_SETFL, flags);

        // close-on-exec
        flags = ::fcntl(sockfd, F_GETFD, 0);
        flags |= FD_CLOEXEC;
        ::fcntl(sockfd, F_SETFD, flags);
    }

    int getSocketError(int sockfd)
    {
        int optval;
        socklen_t optlen = static_cast<socklen_t>(sizeof(optval));

        if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
        {
            return errno;
        }
        else
        {
            return optval;
        }
    }

    struct sockaddr_in getLocalAddr(int sockfd)
    {
        struct sockaddr_in localaddr;
        ::bzero(&localaddr, sizeof(localaddr));
        socklen_t addrlen = static_cast<socklen_t>(sizeof(localaddr));
        if (::getsockname(sockfd, (struct sockaddr *)&localaddr, &addrlen) < 0)
        {
            LOG_ERROR("sockets_ops::getLocalAddr()");
        }
        return localaddr;
    }

    struct sockaddr_in getPeerAddr(int sockfd)
    {
        struct sockaddr_in peeraddr;
        ::bzero(&peeraddr, sizeof(peeraddr));
        socklen_t addrlen = static_cast<socklen_t>(sizeof(peeraddr));
        if (::getpeername(sockfd, (struct sockaddr *)&peeraddr, &addrlen) < 0)
        {
            LOG_ERROR("sockets_ops::getPeerAddr()");
        }
        return peeraddr;
    }

}