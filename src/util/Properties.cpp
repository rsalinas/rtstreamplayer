#include "Properties.h"

#include <fstream>
#include <vector>
#include <sstream>

using namespace std;

static vector<string> splitString(const string& s, char delim) {
    vector<string> result;
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) {
        result.push_back(item);
    }
    return result;
}

Properties::Properties(const std::string& file) {
    ifstream is{file};
    char line[1024];
    while (is.getline(line, sizeof line)) {
        string s{line};
        auto equalPos = s.find("=");
        if (equalPos != s.npos) {
            data_[s.substr(0, equalPos)] = s.substr(equalPos+1);
        }
    }
}

std::string Properties::getString(const std::string& key, const std::string& defaultValue) const {
    if (data_.find(key) != data_.end()) {
        return data_.at(key).c_str();
    } else {
        return defaultValue;
    }
}

int Properties::getInt(const std::string& key, int defaultValue) const {
    if (data_.find(key) != data_.end()) {
        return atoi(data_.at(key).c_str());
    } else {
        return defaultValue;
    }
}

