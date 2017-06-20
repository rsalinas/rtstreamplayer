#pragma once

class Popen
{
    FILE * f = nullptr;
public:
    Popen(const char *cmd, const char *mode) : f(popen(cmd, mode)) {

    }

    FILE * getFile() const {
        return f;
    }

    ~Popen() {
        pclose(f);
    }
};
