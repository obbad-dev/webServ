#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>

struct FdManager;
class LocationConf;
class Server;

class HttpResponseBuilder
{
public:
    static void build(FdManager &manager);

private:
    static const LocationConf* findLocation(const std::string &requestPath, const Server &server);
    static std::string generateDirectoryListing(const std::string &dirPath, const std::string &uriPath);
    static bool readBinaryFile(const std::string &filepath, std::string &content);
    static void executeCGI(FdManager &manager, const std::string &physicalPath);
};
