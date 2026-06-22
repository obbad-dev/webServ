#pragma once
using namespace std;

#include <string>

#include "ParseConfig.hpp"
#include "HttpRequest.hpp"

class ServerSide
{
private:
    const vector<Server> &servers;
    HttpRequest httpRequest;
    void debug();

public:
    ServerSide(const vector<Server> &servers);
    ~ServerSide();

    int setup();
};

