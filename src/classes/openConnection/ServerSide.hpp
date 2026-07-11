#pragma once
using namespace std;

#include <string>
#include <sys/epoll.h>
#include <cerrno>

#include "ParseConfig.hpp"
#include "HttpRequest.hpp"

struct FdManager
{
    int fd;
    time_t lastActivity;
    size_t bytesSent;
    string type;
    // HttpRequest request;
    // HttpResponse response;
    string recvBuffer;
    string sendBuffer;
};

class ServerSide
{
private:
    const vector<Server> &servers;
    // map<int, string> fds;
    map<int, FdManager> fds;
    map<int, HttpRequest> httpRequests;
    void debug();

public:
    ServerSide(const vector<Server> &servers);
    ~ServerSide();

    void setup();
    void create_server_sock();
    void communication_part();
};

