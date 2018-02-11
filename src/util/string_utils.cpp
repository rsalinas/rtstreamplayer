#include "string_utils.h"

#include <sstream>
#include <fstream>

using namespace std;

std::vector<string> splitString(const std::string& s, char delim) {
    std::vector<std::string> result;
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) {
        result.push_back(item);
    }
    return result;
}

bool startsWith(const std::string& hay, const std::string& needle) {
    return  hay.compare(0, needle.length(), needle) == 0;
}

std::string paren(const std::string& s) {
    return string{"("} + s + ")";
}

std::string readFileFully(const std::string& filename) {
    std::ifstream t(filename);
    std::string str((std::istreambuf_iterator<char>(t)),
                    std::istreambuf_iterator<char>());
    return str;
}

std::string secondsToHuman(int seconds) {
    auto x = seconds;
    auto secs = x % 60;
    x /= 60;
    auto minutes = x % 60;
    x /= 60;
    auto hours = x % 24;
    x /= 24;
    auto days = x;
    stringstream ss;

    if (days) {
        ss << days << " days";
    }
    if (hours) {
        ss << " " << hours << " hours";
    }
    if (minutes) {
        ss << " " << minutes<< " mins";
    }
    if (secs) {
        ss << " " << secs << " secs";
    }

    return ss.str();
}
