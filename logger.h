#pragma once

#include <stdio.h>
#include <string>

#include "noncopyable.h"

// 定义5种宏,

#ifdef MUDUO_DBG
#define LOG_DEBUG(logmsgFormat, ...)                     \
    do                                                   \
    {                                                    \
        Logger &logger = Logger::getInstance();          \
        logger.setLogLevel(LogLevel::DEBUG);             \
        char buf[1024] = {0};                            \
        snprintf(buf, 1024, logmsgFormat, ##_VA_ARGS__); \
        Logger.log(buf);                                 \
    } while (0);
#else
#define LOG_DEBUG(logmsgFormat, ...)
#endif

#define LOG_INFO(logmsgFormat, ...)                       \
    do                                                    \
    {                                                     \
        Logger &logger = Logger::getInstance();           \
        logger.setLogLevel(LogLevel::INFO);               \
        char buf[1024] = {0};                             \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                                  \
    } while (0);

#define LOG_WARNNING(logmsgFormat, ...)                   \
    do                                                    \
    {                                                     \
        Logger &logger = Logger::getInstance();           \
        logger.setLogLevel(LogLevel::WARNNING);           \
        char buf[1024] = {0};                             \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                                  \
    } while (0);

#define LOG_ERROR(logmsgFormat, ...)                      \
    do                                                    \
    {                                                     \
        Logger &logger = Logger::getInstance();           \
        logger.setLogLevel(LogLevel::ERROR);              \
        char buf[1024] = {0};                             \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                                  \
    } while (0);

#define LOG_FATAL(logmsgFormat, ...)                      \
    do                                                    \
    {                                                     \
        Logger &logger = Logger::getInstance();           \
        logger.setLogLevel(LogLevel::FATAL);              \
        char buf[1024] = {0};                             \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                                  \
        exit(-1);                                         \
    } while (0);

//////////////////////////////////////
////

enum LogLevel
{
    DEBUG,
    INFO,
    WARNNING,
    ERROR,
    FATAL // coredump
};

// 日之类, 单例,
class Logger : noncopyable
{
public:
    static Logger &getInstance();

    void setLogLevel(int level);

    void log(const std::string &msg);

private:
    Logger() {}
    ~Logger() {}

private:
    int logLevel_;
};
