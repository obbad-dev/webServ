#include "ServerSide.hpp"
#include <string>
#include <map>

std::string getMimeType(std::string& path, std::string msg)
{
    std::string substring;
    size_t pos = path.rfind(".");
    if (pos != std::string::npos)
        substring = path.substr(pos);
    else
        return msg;

    std::map<std::string, std::string>::iterator it = FdManager::extensions.find(substring);
    if (it != FdManager::extensions.end())
        return it->second;
    else
        return msg;
}
