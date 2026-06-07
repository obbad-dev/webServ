#include "Server.hpp"

#include <cerrno>
#include <cstdlib>
#include <stdexcept>

Server::Server() : _port(-1), _root("")
{
}

Server::~Server()
{
}

void Server::setPort(string port)
{
    char *end = NULL;
    long value = 0;

    errno = 0;
    value = strtol(port.c_str(), &end, 10);

    if (end == port.c_str() || *end != '\0')
        throw invalid_argument("Port is not a valid number: " + port);

    if (errno == ERANGE)
        throw out_of_range("Port number overflow: " + port);

    if (value < 1 || value > 65535)
        throw out_of_range("Port must be between 1 and 65535: " + port);

    this->_port = static_cast<int>(value);
}

void Server::setRoot(string root)
{
    this->_root = root;
}

const int &Server::getPort() const
{
    return this->_port;
}

const string &Server::getRoot() const
{
    return this->_root;
}
