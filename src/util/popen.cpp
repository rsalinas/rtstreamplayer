#include "popen.h"

#include <array>

using namespace std;

std::string execCmd(const char* cmd) {

    std::array<char, 4096> buffer;
    std::string result;
    std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (!feof(pipe.get())) {
        if (fgets(buffer.data(), 4096, pipe.get()) != nullptr)
            result += buffer.data();
    }
    return result;
}

std::string execCmd(const std::string& cmd) {
    return execCmd(cmd.c_str());
}
