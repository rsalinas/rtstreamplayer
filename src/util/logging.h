#pragma once

#include <sstream>
#include <iostream>
#include <chrono>
#include <SDL/SDL_audio.h>


std::string nowStr( const char* format = "%F %T" );


std::ostream & operator<<(std::ostream & os, const SDL_AudioSpec obtained);

enum LogLevel {
    ERROR, WARN, INFO, DEBUG
};

#define LOG_INFO() LoggerProxy{LogLevel::INFO}
#define LOG_WARN() LoggerProxy{LogLevel::WARN}
#define LOG_DEBUG() LoggerProxy{LogLevel::DEBUG}
#define LOG_ERROR() LoggerProxy{LogLevel::ERROR}

class LoggerProxy  {
    const LogLevel logLevel;
    const std::string timestampText;
    std::ostringstream oss;

public:
    LoggerProxy(LogLevel level) : logLevel(level), timestampText(nowStr()) {
    }

    template<typename T>
    std::ostream &operator<<(const T & data) {
        oss << data;
        return oss;
    }

    static const char * toString(LogLevel logLevel) {
        switch (logLevel) {
        case  ERROR:
            return "ERROR";
        case WARN:
            return "WARN";
        case INFO:
            return "INFO";
        case DEBUG:
        default:
            return "DEBUG";
        }
    }

    ~LoggerProxy() {
        std::clog << timestampText << (std::string{": ["}+toString(logLevel)+"] ") << oss.str() << std::endl;
    }
};
