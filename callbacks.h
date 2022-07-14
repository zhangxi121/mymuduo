#pragma once

#include <functional>
#include <memory>

#include "timestamp.h"

class Buffer;
class TcpConnection;

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

using TimerCallback = std::function<void()>;
using ConnectionCallback = std::function<void(const TcpConnectionPtr &)>;
using CloseCallback = std::function<void(const TcpConnectionPtr &)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr &)>;

/**
 * 发送数据的时候, 对端接收的慢, 本端发送的快, 数据就会丢失掉了, 就可能出错,
 * 接收方和发送方两边的速率要趋于接近才是良好的网络状况,
 * 水位控制就是这样一个目的, 到达 水位线了可以暂停发送,  
 */
using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr &, size_t)>;

using MessageCallback = std::function<void(const TcpConnectionPtr &, Buffer *, Timestamp)>;

void defaultConnectionCallback(const TcpConnectionPtr &conn);
void defaultMessageCallback(const TcpConnectionPtr &conn, Buffer *buffer, Timestamp receiveTime);

//
