#include "Server.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <linux/limits.h>
#include <climits>

Server::Server() : _root(""), client_max_body_size(MB_MULTIPLIER), has_set_client_max_body_size(false) 
{
    index.push_back("index.html");
}

Server::~Server()
{
}

void Server::setListen(const std::string &ip_port)
{
    char *endStr = NULL;
    errno = 0;
    Listen listen;
    listen.port = 0;
    listen.ip = "0.0.0.0";

    if (ip_port == ";")
        throw std::invalid_argument("The 'listen' directive is Empty");

    size_t endIp = ip_port.find(":");
    if (endIp != std::string::npos)
    {
        listen.ip = ip_port.substr(0, endIp++);
        if (ip_port.find(":", endIp) != std::string::npos || listen.ip.empty())
            throw std::invalid_argument("The 'listen' directive is not valid: " + ip_port);
        std::string ip = listen.ip + ".";
        size_t start = 0;
        size_t end;
        size_t howNb = 0;
        while ((end = ip.find(".", start)) != std::string::npos)
        {
            errno = 0;
            howNb++;
            std::string nbStr = ip.substr(start, end - start);
            if (nbStr.empty())
                throw std::invalid_argument("The 'listen' directive is not valid: invalid IP address -> " + listen.ip);
            long nb = strtol(nbStr.c_str(), &endStr, 10);
            if (*endStr != '\0' || nb > 255 || nb < 0 || errno == ERANGE)
                throw std::invalid_argument("The 'listen' directive is not valid: invalid IP address -> " + listen.ip);
            end++;
            start = end;
        }
        if (howNb != 4)
            throw std::invalid_argument("The 'listen' directive is not valid: invalid IP address -> " + listen.ip);
        endStr = NULL;
        errno = 0;
    }
    else
    {
        endIp = 0;
    }
    std::string portStr = ip_port.substr(endIp);
    listen.port = strtol(portStr.c_str(), &endStr, 10);
    if (*endStr != '\0' || portStr.empty())
        throw std::invalid_argument("The 'listen' directive is not valid: " + ip_port);
    if (errno == ERANGE)
        throw std::out_of_range("Port number overflow for the 'listen' directive: " + ip_port);
    if (listen.port < 1 || listen.port > 65535)
        throw std::out_of_range("Port must be between 1 and 65535 for the 'listen' directive: " + ip_port);
    std::vector<Listen>::iterator it = std::find(listens.begin(), listens.end(), listen);
    if (it != listens.end())
        throw std::invalid_argument("Listen directive duplicated: " + ip_port);
    listens.push_back(listen);
}
void Server::setRoot(const std::string &pathStr)
{
    if (!_root.empty())
        throw std::runtime_error("duplicate root directive in server block");
    if (pathStr == ";")
        throw std::invalid_argument("The 'root' directive is Empty");

    this->_root = pathStr;
}
void Server::setIndex(const std::vector<std::string>& indexFiles){
    if (indexFiles.empty())
        throw std::invalid_argument("The 'index' directive is Empty");
    this->index.insert(this->index.end(), indexFiles.begin(), indexFiles.end());
}

void Server::setClientMaxBodySize(const std::string &token)
{
    if (has_set_client_max_body_size)
        throw std::runtime_error("duplicate 'client_max_body_size' directive in server block");
    if (token == ";")
        throw std::invalid_argument("The 'client_max_body_size' directive is Empty");

    if (!token.empty() && token[0] == '-' && isdigit(token[1]))
        throw std::invalid_argument("number of client_max_body_size must be positive: " + token);
    char *end;
    errno = 0;
    unsigned long long value = strtoull(token.c_str(), &end, 10);
    if (end == token)
        throw std::invalid_argument("invalid number of client_max_body_size " + token);
    if (errno == ERANGE)
        throw std::invalid_argument("number client_max_body_size overflowed " + token);

    unsigned long long multiplier = 1;
    if (*end != '\0')
    {
        if (*(end + 1) != '\0')
            throw std::invalid_argument("Unsupported unit: " + token + " in client_max_body_size directive");

        char unit = *end;
        if (unit == 'M' || unit == 'm')
            multiplier = MB_MULTIPLIER;
        else if (unit == 'G' || unit == 'g')
            multiplier = GB_MULTIPLIER;
        else if (unit == 'K' || unit == 'k')
            multiplier = KB_MULTIPLIER;
        else
            throw std::invalid_argument("Unsupported unit: " + token + " in client_max_body_size directive");
    }
    if (value > ULLONG_MAX / multiplier)
            throw std::invalid_argument("'client_max_body_size' is too large: " + token);
    this->client_max_body_size = value * multiplier;
    this->has_set_client_max_body_size = true;
}
void Server::setErrorsPages(std::vector<std::string>& errorTokens)
{
    std::string URI = errorTokens.back();
    errorTokens.pop_back();

    for (size_t i = 0; i < errorTokens.size(); i++)
    {
        char *end;
        errno = 0;
        std::string nbStr = errorTokens[i];
        long value = strtol(nbStr.c_str(), &end, 10);
        if (*end != '\0')
            throw std::invalid_argument("number of error not valid "+nbStr);
        else if (errno == ERANGE || value < 400 || value > 599)
            throw std::invalid_argument("number of error page must be in range 400 - 500");
        errors_page[value] = URI;
    }
}
void Server::pushLocation(LocationConf& location){
    locations.push_back(location);
}

const std::vector<Listen> &Server::getListens() const
{
    return listens;
}
const std::string &Server::getRoot() const
{
    return this->_root;
}
const std::vector<std::string> &Server::getIndex() const{
    return this->index;
}
const uint64_t &Server::getClientMaxBodySize() const
{
    return client_max_body_size;
}

const std::map<int, std::string>& Server::getErrorsPages() const {
    return this->errors_page;
}
const std::vector<LocationConf> &Server::getLocations() const
{
    return this->locations;
}
std::vector<LocationConf>& Server::getForModifyLocation(){
    return this->locations;
}
