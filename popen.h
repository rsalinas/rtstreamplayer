#pragma once

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
        logInfo("closed source with exit value: " + std::to_string(pclose(f)));
    }
};
