
#include <iostream>
#include <stdio.h>

#include "logger.h"
#include "timestamp.h"


//

Logger &Logger::getInstance()
{
    static Logger logger;
    return logger;
}

void Logger::setLogLevel(int level)
{
    logLevel_ = level;
}

// 这里打印在控制台,
void Logger::log(const std::string &msg)
{
    switch (logLevel_)
    {
    case LogLevel::DEBUG:
        std::cout << "[DEBUG]";
        break;
    case LogLevel::INFO:
        std::cout << "[INFO]";
        break;

    case LogLevel::WARNNING:
        std::cout << "[WARNNING]";
        break;

    case LogLevel::ERROR:
        std::cout << "[ERROR]";
        break;

    case LogLevel::FATAL:
        std::cout << "[FATAL]";
        break;

    default:
        break;
    }

    // 打印时间和msg,
    std::cout << Timestamp::now().toString() << " : " << msg << std::endl;
}
