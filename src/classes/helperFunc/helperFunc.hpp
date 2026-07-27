#pragma once
#include <string>
#include <vector>
#include "LocationConf.hpp"


bool realPath(const std::string& root, const std::string& uri, std::string& result);
std::string getMimeType(std::string& path, std::string msg);
std::string intToString(int number);
const LocationConf* getMatchingLocation(const std::vector<LocationConf>& locations, const std::string& path);