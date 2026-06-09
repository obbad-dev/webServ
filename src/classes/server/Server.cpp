#include "Server.hpp"

#include <cerrno>
#include <cstdlib>
#include <stdexcept>
#include <iostream>
#include <algorithm>

Server::Server() : _root("")
{
}

Server::~Server()
{
}

void Server::setListen(string ip_port)
{
    char *endStr = NULL;
    errno = 0;
    Listen listen;
    listen.port = 0;
    listen.ip = "0.0.0.0";

    if (ip_port.empty())
        throw invalid_argument("The 'listen' directive is not valid: " + ip_port);

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
    else{
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

void Server::setRoot(string root)
{
    this->_root = root;
}

const vector<Listen> &Server::getListens() const
{
    return listens;
}

const string &Server::getRoot() const
{
    return this->_root;
}
