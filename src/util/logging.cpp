#include "logging.h"

std::string nowStr(const char* format)
{
    std::time_t t = std::time(0);
    char cstr[128];
    std::strftime( cstr, sizeof(cstr), format, std::localtime(&t) ) ;
    return cstr;
}

std::ostream & operator<<(std::ostream & os, const SDL_AudioSpec obtained) {
    return os << obtained.freq << " " << int(obtained.channels) << " chan, " << obtained.samples << " samples" ;
}
