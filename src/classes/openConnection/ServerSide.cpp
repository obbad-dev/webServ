#include "ServerSide.hpp"

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

ServerSide::ServerSide(ParseConfig &config) : _config(config)
{
}

ServerSide::~ServerSide()
{
}

string ServerSide::readRequest(int &clientFd)
{
    string req;
    char buffer[4096];

    while (true)
    {
        memset(buffer, 0, sizeof(buffer));
        ssize_t byteRead = recv(clientFd, buffer, sizeof(buffer), 0);

        if (byteRead < 0)
        {
            perror("Error");
            throw runtime_error("");
        }
        if (byteRead == 0)
            break;
        req.append(buffer, static_cast<size_t>(byteRead));
    }

    return req;
}

std::string ServerSide::parseRequest(std::string &buffer)
{
    size_t start = 0;
    size_t end = 0;

    // std::cout << "------------------------" << 
    while ((end = buffer.find("\r\n", start)) != string::npos)
    {
        if (start == 0)
            
        // std::cout << buffer.substr(start, end - start) << std::endl;
        start = end + 2;
    }
    // std::cout << "------------------------" << std::endl;
    return "";
}

void ServerSide::setup()
{
    const Server &server = _config.getSrvers()[0];
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);

    if (serverFd < 1)
    {
        perror("socket");
        throw runtime_error("");
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(server.getPort());

    if (bind(serverFd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0)
    {
        perror("bind");
        throw runtime_error("");
    }

    if (listen(serverFd, 10))
    {
        perror("listen");
        throw runtime_error("");
    }

    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);
    int clientFd = accept(serverFd, reinterpret_cast<struct sockaddr *>(&clientAddr), &clientLen);

    if (clientFd < 0)
    {
        perror("accept");
        throw runtime_error("");
    }

    string buffer = readRequest(clientFd);
    parseRequest(buffer);

    string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: "
        "Content-Type: text/plain\r\n"
        "\r\n"
        "Hello, World!";

    write(clientFd, response.c_str(), response.length());
    close(clientFd);
    close(serverFd);
}
