#include "string_utils.h"
#include <sstream>

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
