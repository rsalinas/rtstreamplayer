#include "popen.h"

class SimpleCommandExecutor {
    Popen shellProcess{"exec /bin/sh -x", "w"};
public:
    void runCommand(const std::string & cmd) {
        fputs((cmd+"\n").c_str(), shellProcess.getFile());
        fflush(shellProcess.getFile());
    }

};
