#pragma once
#include <string>
#include <vector>
#include "LocationConf.hpp"
#include <map>
#include <string>

struct FdManager;
#define SIZE_BUFFER 65536

bool realPath(const std::string& root, const std::string& uri, std::string& result);
std::string getMimeType(std::string& path, std::string msg);
std::string intToString(int number);
const LocationConf* getMatchingLocation(const std::vector<LocationConf>& locations, std::string& path);
void cleanupCgi(FdManager &manager, std::map<int, int> &cgiToClient);