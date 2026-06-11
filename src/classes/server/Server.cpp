#include "Server.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <linux/limits.h>
#include <climits>

Server::Server() : _root(""), client_max_body_size(1048576), has_set_client_max_body_size(false) 
{
}

Server::~Server()
{
}

void Server::setListen(string &ip_port)
{
    char *endStr = NULL;
    errno = 0;
    Listen listen;
    listen.port = 0;
    listen.ip = "0.0.0.0";

    if (ip_port == ";")
        throw invalid_argument("The 'listen' directive is Empty");

    size_t endIp = ip_port.find(":");
    if (endIp != string::npos)
    {
        listen.ip = ip_port.substr(0, endIp++);
        if (ip_port.find(":", endIp) != string::npos || listen.ip.empty())
            throw invalid_argument("The 'listen' directive is not valid: " + ip_port);
        string ip = listen.ip + ".";
        size_t start = 0;
        size_t end;
        size_t howNb = 0;
        while ((end = ip.find(".", start)) != string::npos)
        {
            errno = 0;
            howNb++;
            string nbStr = ip.substr(start, end - start);
            if (nbStr.empty())
                throw invalid_argument("The 'listen' directive is not valid: invalid IP address -> " + listen.ip);
            long nb = strtol(nbStr.c_str(), &endStr, 10);
            if (*endStr != '\0' || nb > 255 || nb < 0 || errno == ERANGE)
                throw invalid_argument("The 'listen' directive is not valid: invalid IP address -> " + listen.ip);
            end++;
            start = end;
        }
        if (howNb != 4)
            throw invalid_argument("The 'listen' directive is not valid: invalid IP address -> " + listen.ip);
        endStr = NULL;
        errno = 0;
    }
    else
    {
        endIp = 0;
    }
    string portStr = ip_port.substr(endIp);
    listen.port = strtol(portStr.c_str(), &endStr, 10);
    if (*endStr != '\0' || portStr.empty())
        throw invalid_argument("The 'listen' directive is not valid: " + ip_port);
    if (errno == ERANGE)
        throw out_of_range("Port number overflow for the 'listen' directive: " + ip_port);
    if (listen.port < 1 || listen.port > 65535)
        throw out_of_range("Port must be between 1 and 65535 for the 'listen' directive: " + ip_port);
    vector<Listen>::iterator it = find(listens.begin(), listens.end(), listen);
    if (it != listens.end())
        throw invalid_argument("Listen directive duplicated: " + ip_port);
    listens.push_back(listen);
}
void Server::setRoot(string &pathStr)
{
    if (!_root.empty())
        throw runtime_error("duplicate root directive in server block");
    if (pathStr == ";")
        throw invalid_argument("The 'root' directive is Empty");

    this->_root = pathStr;
}

void Server::setClientMaxBodySize(string& token)
{
    if (has_set_client_max_body_size)
        throw runtime_error("duplicate 'client_max_body_size' directive in server block");
    if (token == ";")
        throw invalid_argument("The 'client_max_body_size' directive is Empty");

    if (!token.empty() && token[0] == '-' && isdigit(token[1]))
        throw invalid_argument("number of client_max_body_size must be positive: " + token);
    char *end;
    errno = 0;
    unsigned long long value = strtoull(token.c_str(), &end, 10);
    if (end == token)
        throw invalid_argument("invalid number of client_max_body_size " + token);
    if (errno == ERANGE)
        throw invalid_argument("number client_max_body_size overflowed " + token);

    unsigned long long multiplier = 1;
    if (*end != '\0')
    {
        if (*(end + 1) != '\0')
            throw invalid_argument("Unsupported unit: " + token + " in client_max_body_size directive");

        char unit = *end;
        if (unit == 'M' || unit == 'm')
            multiplier = MB_MULTIPLIER;
        else if (unit == 'G' || unit == 'g')
            multiplier = GB_MULTIPLIER;
        else if (unit == 'K' || unit == 'k')
            multiplier = KB_MULTIPLIER;
        else
            throw invalid_argument("Unsupported unit: " + token + " in client_max_body_size directive");
    }
    if (value > ULLONG_MAX / multiplier)
            throw std::invalid_argument("'client_max_body_size' is too large: " + token);
    this->client_max_body_size = value * multiplier;
    this->has_set_client_max_body_size = true;
}
// void Server::setErrorsPages(vector<string>& errorTokens)
// {

// }

const vector<Listen> &Server::getListens() const
{
    return listens;
}
const string &Server::getRoot() const
{
    return this->_root;
}
const uint64_t &Server::getClientMaxBodySize() const
{
    return client_max_body_size;
}
const bool& Server::hasSetClientMaxBodySize() const
{
    return  has_set_client_max_body_size;
}
const map<int, string>& Server::getErrorsPages() const {
    return this->errors_page;
}