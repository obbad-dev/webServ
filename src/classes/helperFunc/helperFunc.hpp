#pragma once
#include <string>

bool realPath(const std::string& root, const std::string& uri, std::string& result);
std::string getMimeType(std::string& path, std::string msg);
std::string intToString(int number);
