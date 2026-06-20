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

void prepare_extensions_map(map<string, string> &extensions)
{
    extensions[".html"] = "text/html";
    extensions[".css"] = "text/css";
    extensions[".js"] = "application/javascript";
    extensions[".jpg"] = "image/jpeg";
    extensions[".jpeg"] = "image/jpeg";
    extensions[".png"] = "image/png";
    extensions[".gif"] = "image/gif";
    extensions[".ico"] = "image/x-icon";
    extensions[".txt"] = "text/plain";
    extensions[".pdf"] = "application/pdf";
}

int ServerSide::setup()
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
        return (perror("socket"), 1);

    struct sockaddr_in s_addr, c_addr;
    bzero(&s_addr, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(8080);
    s_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, reinterpret_cast<sockaddr*>(&s_addr), sizeof(s_addr)) == -1)
        return (perror("bind"), close(sockfd), 1);

    if (listen(sockfd, 5) == -1)
        return (perror("listen"), close(sockfd), 1);

    socklen_t c_len = sizeof(c_addr);

    prepare_extensions_map(httpRequest.extensions);

    while (1)
    {
        int client_fd = accept(sockfd, reinterpret_cast<sockaddr*>(&c_addr), &c_len);
        if (client_fd == -1)
            return(perror("accept"), close(sockfd), 1);

        httpRequest.parseRequest(client_fd);

        httpRequest.create_response(client_fd);

        close(client_fd);
    }
    close(sockfd);
}
