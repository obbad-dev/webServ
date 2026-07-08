#pragma once
using namespace std;

#include <string>
#include <sys/epoll.h>

#include "ParseConfig.hpp"
#include "HttpRequest.hpp"

class ServerSide
{
private:
    const vector<Server> &servers;
    map<int, string> fds;
    map<int, HttpRequest> httpRequests;
    void debug();

public:
    ServerSide(const vector<Server> &servers);
    ~ServerSide();

    void setup();
    void create_server_sock();
    void communication_part();
};

