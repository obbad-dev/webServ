#pragma once
using namespace std;

#include <string>
#include <sys/epoll.h>
#include <cerrno>

#include "ParseConfig.hpp"
#include "HttpRequest.hpp"

#define TIMEOUT 30
enum CONN_TYPE {SERVER, CLIENT};

enum TYPE { SERVER, CLIENT };
struct FdManager
{
    time_t lastActivity;
    HttpRequest request;
    CONN_TYPE type;
    // HttpResponse response;
    size_t bytesSent;
    string recvBuffer;
    string sendBuffer;
    const Server &blockServer;

    // FdManager(void){}

    FdManager(CONN_TYPE _type, time_t _lastActivity, const Server &_blockServer) : blockServer(_blockServer)
    {
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

    map<string, string> extensions;

    void setup();
    void create_server_sock();
    void communication_part();
};

