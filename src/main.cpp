#include "logging.h"

#include "rtstreamplayer.h"

static std::unique_ptr<RtStreamPlayer> instance;

void signalHandler(int sig) {
    LOG_DEBUG()  << __FUNCTION__ << " " << strsignal(sig) ;
    switch (sig) {
    case SIGUSR1:
        break;
    case SIGUSR2:
        break;
    case SIGINT:
    case SIGTERM:
        instance->pleaseFinish();
        break;
    }
}

class SignalHandler {
    const int sig;
public:
    SignalHandler(int sig, sighandler_t handler) : sig(sig) {
        LOG_DEBUG()  << __FUNCTION__ << " " << strsignal(sig) ;
        signal(sig, handler);
    }
    ~SignalHandler() {
        LOG_DEBUG()  << __FUNCTION__ << " " << strsignal(sig) ;
        signal(sig, SIG_DFL);

    }
};



int main(int argc, char **argv)
{

    LOG_INFO() << "rtsp";
    try {
        instance.reset(new RtStreamPlayer);
        auto sigUsrHandler =  [](int sig) {
            LOG_INFO()  << __FUNCTION__ << " " << strsignal(sig) ;
            //instance->sigUSR1();
        };

        SignalHandler usr1Handler{SIGUSR1, sigUsrHandler};
        SignalHandler usr2Handler{SIGUSR2, sigUsrHandler};

        auto finishHandler = [](int sig) {
            LOG_DEBUG()  << __FUNCTION__ << " " << strsignal(sig) ;
            instance->pleaseFinish();
        };

        SignalHandler intHandler{SIGINT, finishHandler};
        SignalHandler termHandler{SIGTERM, finishHandler};

        instance->run();
    } catch (const std::exception &e) {
        LOG_INFO()  << "Exception: " << e.what() ;
    }
}