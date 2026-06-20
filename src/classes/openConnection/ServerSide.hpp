#pragma once
using namespace std;

#include <string>

#include "ParseConfig.hpp"
#include "HttpRequest.hpp"

class ServerSide
{
private:
    ParseConfig &_config;
    HttpRequest httpRequest;
    void debug();

public:
    ServerSide(ParseConfig &config);
    ~ServerSide();

    int setup();
};

