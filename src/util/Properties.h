#pragma once

#include <string>
#include <map>

class Properties {
public:
    Properties(const std::string& file);
    std::string getString(const std::string& key, const std::string& defaultValue = "") const;
    int getInt(const std::string& key, int defaultValue = 0) const;

private:
    std::map<std::string, std::string> data_;
};
