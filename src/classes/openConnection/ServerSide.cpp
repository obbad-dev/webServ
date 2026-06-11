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


void ServerSide::debug()
{
    cout << "Method: " << this->httpRequest.getMethod() << '\n';
    cout << "Target: " << this->httpRequest.getPath() << '\n';
    cout << "Protocol: " << this->httpRequest.getProtocolVersion() << '\n';
    cout << "------------Headers-----------" << '\n';
    const map<string, string> &headers = this->httpRequest.getHeaders();
    for (map<string, string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
        cout << it->first << ": " << it->second << '\n';
}

void ServerSide::setup()
{
    // const Server &server = _config.getSrvers()[0];
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
    addr.sin_port = htons(8080);

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
    while (1)
    {
        int clientFd = accept(serverFd, reinterpret_cast<struct sockaddr *>(&clientAddr), &clientLen);
        if (clientFd < 0)
        {
            perror("accept");
            throw runtime_error("");
        }
    
        httpRequest.parseRequest(clientFd);
        debug();
    
        std::string response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 13\r\n" // <-- Added \r\n here
            "Content-Type: text/plain\r\n"
            "\r\n" // End of headers blank line
            "Hello, World!";
    
        send(clientFd, response.c_str(), response.length(), 0);
    
        // Warning: Be careful with closing serverFd here if you are in a loop!
        close(clientFd);
    }
    close(serverFd);
}
