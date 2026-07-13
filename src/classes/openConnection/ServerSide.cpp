#include "ServerSide.hpp"

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>

#include <arpa/inet.h> 

ServerSide::ServerSide(const vector<Server> &servers) : servers(servers)
{
}

ServerSide::~ServerSide()
{
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


void ServerSide::create_server_sock()
{
    for (size_t i = 0; i < servers.size(); i++)
    {
        const vector<Listen> &tmp_listen = servers[i].getListens();

        for (size_t j = 0; j < tmp_listen.size(); j++)
        {
            int sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd == -1)
                throw runtime_error("Socket failed to open"); // close previous files when error occurs

            int opt = 1;
            if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
                throw runtime_error("Setsockopt failed");

            if (fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1)
                throw runtime_error("Fcntl for server failed");

            struct sockaddr_in s_addr;
            bzero(&s_addr, sizeof(s_addr)); // replace with our bzero
            s_addr.sin_family = AF_INET;
            s_addr.sin_port = htons(tmp_listen[j].port);
            s_addr.sin_addr.s_addr = inet_addr(tmp_listen[j].ip.c_str()); // inet_addr not allowed

            if (bind(sockfd, reinterpret_cast<sockaddr*>(&s_addr), sizeof(s_addr)) == -1)
                throw runtime_error("Bind failed"); // close previous files when error occurs

            if (listen(sockfd, SOMAXCONN) == -1)
                throw runtime_error("listen failed"); // close previous files when error occurs

            fds[sockfd] = FdManager("Server", 0);
        }
    }
}

void add_fd_to_epoll(int epoll_fd, int fd, uint32_t events)
{
    struct epoll_event ev;

    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1)
        throw runtime_error("Epoll_ctl_add failed");
}

// void close_fds(map<int, string> fds)
// {
//     for (map<int, string>::iterator it = fds.begin(); it != fds.end(); it++)
//     {
//         close (it->first);
//     }
// }

void remove_from_epoll(int epoll_fd, int fd)
{
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) == -1)
        throw runtime_error("Epoll_ctl_del failed");
}

void ServerSide::communication_part()
{
    int epoll_fd = epoll_create(1);

    if (epoll_fd == -1)
        throw runtime_error("Epoll creation failed");

    for (map<int, FdManager>::iterator it = fds.begin(); it != fds.end(); it++)
    {
        add_fd_to_epoll(epoll_fd, it->first, EPOLLIN);
    }

    struct epoll_event event_arr[1024];

    while (true)
    {
        int epoll_ready = epoll_wait(epoll_fd, event_arr, 1024, 1000);

        if (epoll_ready == -1)
            throw runtime_error("Epoll wait failed");
        else if (epoll_ready > 0)
        {
            for (int i = 0; i < epoll_ready; i++)
            {
                map<int, FdManager>::iterator it = fds.find(event_arr[i].data.fd);

                if (it->second.type == "Server")
                {
                    while (true)
                    {
                        int clientfd = accept(it->first, NULL, NULL);

                        if (clientfd == -1)
                        {
                            if (errno == EAGAIN || errno == EWOULDBLOCK)
                                break;

                            throw runtime_error("Accept failed");
                        }

                        if (fcntl(clientfd, F_SETFL, O_NONBLOCK) == -1)
                            throw runtime_error("Fcntl for client failed");

                        add_fd_to_epoll(epoll_fd, clientfd, EPOLLIN);

                        cout << "Accepted client fd = " << clientfd << endl;

                        fds[clientfd] = FdManager("Client", time(NULL));
                    }
                }
                else
                {
                    // if (event_arr[i].events == EPOLLIN)
                    // {
                    //     httpRequests[event_arr[i].data.fd].parseRequest(event_arr[i].data.fd);
                    //     httpRequests[event_arr[i].data.fd].create_response(event_arr[i].data.fd);
                    // }

                    it->second.request.parseRequest(it->first);
                    it->second.request.create_response(it->first);
                }
            }
        }
        {
            time_t currentTime = time(NULL);
            for (map<int, FdManager>::iterator it = fds.begin(); it != fds.end(); )
            {
                if (it->second.type == "Client" && (currentTime - it->second.lastActivity) > TIMEOUT)
                {
                    cout << "Disconnecting client with fd = " << it->first << " because timeout = " << (currentTime - it->second.lastActivity) << endl;
                    remove_from_epoll(epoll_fd, it->first);
                    close (it->first);
                    map<int, FdManager>::iterator tmp = it++;
                    fds.erase(tmp);
                }
                else
                    it++;
            }
        }
    }
}

void ServerSide::setup()
{
    create_server_sock();

    // prepare_extensions_map(httpRequest.extensions);

    communication_part();
}
