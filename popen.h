#pragma once

#include <cstdio>
#include "logging.h"


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
