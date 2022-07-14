#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>

class InetAddress
{
public:
    explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1");
    explicit InetAddress(const struct sockaddr_in &addr) : addr_(addr) {}
    ~InetAddress() {}

public:
    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t toPort() const;

    const struct sockaddr_in *getSockAddr() const
    {
        return &addr_;
    }

    void setSockAddrInet(const struct sockaddr_in &addr) { addr_ = addr; }

private:
    struct sockaddr_in addr_;
};
