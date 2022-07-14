#pragma once

#include <algorithm>
#include <assert.h>
#include <string>
#include <vector>

/**
 * 网络库底层的缓冲区类型定义, 
 *
 */

class Buffer
{
public:
    /// A buffer class modeled after org.jboss.netty.buffer.ChannelBuffer
    ///
    /// @code
    /// +-------------------+------------------+------------------+
    /// | prependable bytes |  readable bytes  |  writable bytes  |
    /// |                   |     (CONTENT)    |                  |
    /// +-------------------+------------------+------------------+
    /// |                   |                  |                  |
    /// 0      <=      readerIndex   <=   writerIndex    <=     size
    /// @endcode
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

public:
    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize),
          readerIndex_(kCheapPrepend),
          writerIndex_(kCheapPrepend)
    {
        assert(readableBytes() == 0);
        assert(writableBytes() == initialSize);
        assert(prependableBytes() == kCheapPrepend);
    }

    ~Buffer() = default;

public:
    void swap(Buffer &rhs)
    {
    }

    size_t readableBytes() const
    {
        return writerIndex_ - readerIndex_;
    }

    size_t writableBytes() const
    {
        return buffer_.size() - writerIndex_;
    }

    size_t prependableBytes() const
    {
        return readerIndex_;
    }

    // 返回缓冲区中可读数据的起始地址,
    const char *peek() const
    {
        return begin() + readerIndex_;
    }

    void retrieve(size_t len)
    {
        if (len <= readableBytes())
        {
            // 应用值读取了可读缓冲区数据的一部分, 就是 len 长度, 还剩下 readableBytes() - len 的长度没有读取,
            readerIndex_ += len;
        }
        else
        {
            retrieveAll();
        }
    }

    void retrieveAll()
    {
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }

    // 把 onMessage() 函数上报的 Buffer 数据转成 string 类型的数据, 返回给应用,
    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes());
    }

    // 把 onMessage() 函数上报的 Buffer 数据转成 string 类型的数据, 返回给应用,
    std::string retrieveAsString(size_t len)
    {
        assert(len <= readableBytes());
        std::string result(peek(), len);
        retrieve(len);
        return result;
    }

    void append(const char *data, size_t len)
    {
        ensureWritableBytes(len);
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }

    void append(const void *data, size_t len)
    {
        append(static_cast<const char *>(data), len);
    }

    void ensureWritableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            makeSpace(len);
        }
    }

    char *beginWrite()
    {
        return begin() + writerIndex_;
    }

    const char *beginWrite() const
    {
        return begin() + writerIndex_;
    }

    /**
     * 从 fd 读取数据, 放入到 buffer 中, 
     * Poller 是工作在 LT 模式, fd上的数据没有读取完的话, 底层的 Poller 会不断的上报,
     * Buffer缓冲区是有数据的, 但是从 fd 上读数据的时候, 是不知道TCP数据的长度,
     * 
     */
    ssize_t readFd(int fd, int* savedErrno);

    ssize_t writeFd(int fd, int* savedErrno);
private:
    char *begin()
    {
        // it.operator*()
        return &*buffer_.begin();
    }
    const char *begin() const
    {
        return &*buffer_.begin();
    }

    void makeSpace(size_t len)
    {
        // |  kCheapPrepend  |  readerIndex_  |  writerIndex_  |
        //
        // if(writableBytes()  + prependableBytes() < len + kCheapPrepend){}
        if (len - writerIndex_ - (readerIndex_ - kCheapPrepend))
        {
            /**
             * writerIndex_ 保证了前面的 readBuf()  len 保证了 writeBuf(), 所以(writerIndex_ + len) 就是整个缓冲区大小,
             * 这样一点也不浪费, 读取完 len 的数据后, 缓冲区全部用完了, 此时 writableBytes() ==0,
             * 下次还有数据要来的时候由于  writableBytes() ==0, 就需要重新分配内存空间,
             */        
            buffer_.resize(writerIndex_ + len);
        }
        else
        {
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_, begin() + writerIndex_, begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

private:
    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};
