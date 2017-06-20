#include "popen.h"

static Popen shellProcess{"exec /bin/sh -x", "w"};

void runCommand(const std::string & cmd) {
    fputs((cmd+"\n").c_str(), shellProcess.getFile());
    fflush(shellProcess.getFile());
}
