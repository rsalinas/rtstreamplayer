#pragma once

#include <vector>
#include <string>

std::vector<std::string> splitString(const std::string& s, char delim);
bool startsWith(const std::string& hay, const std::string& needle);

std::string paren(const std::string& s);

std::string readFileFully(const std::string& filename);

std::string secondsToHuman(int seconds);
