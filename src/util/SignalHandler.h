#pragma once

class SignalHandler {
    const int sig;
public:
    SignalHandler(int sig, sighandler_t handler) : sig(sig) {
//        LOG_DEBUG()  << __FUNCTION__ << " " << strsignal(sig) ;
        signal(sig, handler);
    }
    ~SignalHandler() {
//        LOG_DEBUG()  << __FUNCTION__ << " " << strsignal(sig) ;
        signal(sig, SIG_DFL);

    }
};



