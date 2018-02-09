#include "logging.h"

#include "rtstreamplayer.h"
#include <thread>
#include "Properties.h"
#include "SignalHandler.h"

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

int main(int argc, char **argv)
{
    Properties props{"config.props"};
    LOG_INFO() << "rtsp";
    try {
        instance.reset(new RtStreamPlayer{props});
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

        if (!instance->run())
            return EXIT_FAILURE;

        return EXIT_SUCCESS;
    } catch (const std::exception &e) {
        LOG_INFO()  << "Exception: " << e.what() ;
    } catch (...) {
        LOG_INFO()  << "Exception";
    }
}
