#pragma once

#include <netinet/in.h>

namespace sockets_ops
{
    void setNonBlockAndCloseOnExec(int sockfd);

    int getSocketError(int sockfd);

    struct sockaddr_in getLocalAddr(int sockfd);

    struct sockaddr_in getPeerAddr(int sockfd);
}
