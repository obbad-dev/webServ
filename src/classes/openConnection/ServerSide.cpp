#include "ServerSide.hpp"

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>
#include <sstream>

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
    // Ensure full header section was received
    if (buffer.find("\r\n\r\n") == std::string::npos)
        throw std::runtime_error("Incomplete HTTP request");

    size_t start = 0;
    size_t end;

    bool requestLineParsed = false;

    while ((end = buffer.find("\r\n", start)) != std::string::npos)
    {
        std::string line = buffer.substr(start, end - start);

        if (line.empty())
            break;

        if (!requestLineParsed)
        {
            std::istringstream iss(line);

            std::string method;
            std::string target;
            std::string version;
            std::string extra;

            if (!(iss >> method >> target >> version))
                throw std::runtime_error("Invalid request line");

            if (iss >> extra)
                throw std::runtime_error("Too many fields in request line");

            httpRequest.setMethod(method);
            httpRequest.setTarget(target);
            httpRequest.setProtocolVersion(version);
            requestLineParsed = true;
        }
        else
        {
            size_t pos = line.find(':');

            if (pos == std::string::npos)
                throw std::runtime_error("Invalid header: " + line);

            if (pos == 0)
                throw std::runtime_error("Empty header name");

            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            httpRequest.setHeaders(key, value);
        }

        start = end + 2;
    }

    if (httpRequest.getProtocolVersion() == "HTTP/1.1")
    {
        const map<string, string> &hdrs = httpRequest.getHeaders();
        map<string, string>::const_iterator it = hdrs.find("Host");
        if (it == hdrs.end() || it->second == "")
            throw std::runtime_error("Missing Host header");
    }
    debug();
    return "";
}

void ServerSide::debug()
{
    cout << "Method: " << this->httpRequest.getMethod() << '\n';
    cout << "Target: " << this->httpRequest.getPath() << '\n';
    cout << "Protocol: " << this->httpRequest.getProtocolVersion() << '\n';
    cout << "Headers:" << '\n';
    const map<string, string> &headers = this->httpRequest.getHeaders();
    for (map<string, string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
        cout << "  " << it->first << ": " << it->second << '\n';
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
    debug();

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
