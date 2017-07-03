#include <sstream>

std::string nowStr( const char* format = "%F %T" )
{
    std::time_t t = std::time(0);
    char cstr[128];
    std::strftime( cstr, sizeof(cstr), format, std::localtime(&t) ) ;
    return cstr;
}

static void logInfo(const std::string &message) {
    std::clog << nowStr() << ": " << message << std::endl;
}
static void logWarn(const std::string &message) {
    std::clog << nowStr() << ": [WARN] " << message << std::endl;
}

static void logDebug(const std::string &message) {
    std::clog << nowStr() << ": " << message << std::endl;
}

void print(const std::string & message, const SDL_AudioSpec obtained) {
    std::clog << message << ": " << obtained.freq << " " << int(obtained.channels) << " chan, " << obtained.samples << " samples" << std::endl;
}

#define LOG_INFO() LoggerProxy{}

class LoggerProxy  {
    std::ostringstream oss;
public:
    LoggerProxy() {
    }
    template<typename T>
    std::ostream &operator<<(const T & data) {
        std::clog << "op<<" << std::endl;
        oss << data;
        return oss;
    }

    ~LoggerProxy() {
        std::clog << "Finished: " << oss.str() << std::endl;
        oss.str("");
        oss.clear();
    }
};
