#pragma once

#include <cstdio>
#include "logging.h"
#include <array>
#include <memory>

class Popen
{
    FILE * f = nullptr;
public:
    Popen(const char *cmd, const char *mode) : f(popen(cmd, mode)) {
    }

    int getFd() const {
        return fileno(f);
    }
    FILE * getFile() const {
        return f;
    }

    ~Popen() {
        auto retval = pclose(f);
        LOG_INFO()  << "closed source with exit value: " << retval;
    }
};

std::string execCmd(const char* cmd);
std::string execCmd(const std::string& cmd);
