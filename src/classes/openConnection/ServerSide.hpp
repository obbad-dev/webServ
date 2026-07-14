#pragma once
using namespace std;

#include <string>
#include <sys/epoll.h>
#include <cerrno>

#include "ParseConfig.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

#define TIMEOUT 30
enum CONN_TYPE {SERVER, CLIENT};

struct FdManager
{
    time_t lastActivity;
    CONN_TYPE type;
    HttpRequest request;
    HttpResponse response;
    size_t bytesSent;
    string recvBuffer;
    string sendBuffer;
    const Server &blockServer;
    int &epollFd; // to call epoll function from anywhere
    static map<string, string> extensions;

    // FdManager(void){}

    FdManager(CONN_TYPE _type, time_t _lastActivity, const Server &_blockServer, int &_epollFd) : blockServer(_blockServer), epollFd(_epollFd)
    {
        type = _type;
        lastActivity = _lastActivity;
    }
};

class ServerSide
{
private:
    const vector<Server> &servers;
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

