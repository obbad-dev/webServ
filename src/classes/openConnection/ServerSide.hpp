#pragma once
using namespace std;

#include <string>
#include <sys/epoll.h>
#include <cerrno>

#include "ParseConfig.hpp"
#include "HttpRequest.hpp"

#define TIMEOUT 30

enum TYPE { SERVER, CLIENT };
struct FdManager
{
    // int fd;
    time_t lastActivity;
    TYPE type;
    HttpRequest request;
    // HttpResponse response;
    size_t bytesSent;
    string recvBuffer;
    string sendBuffer;

    FdManager(void){}

    FdManager(TYPE _type, time_t _lastActivity)
    {
        // fd = _fd;
        type = _type;
        lastActivity = _lastActivity;
    }
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

