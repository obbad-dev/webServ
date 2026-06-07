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

    string readRequest(int &clientFd);
    string parseRequest(string& buffer);

public:
    ServerSide(ParseConfig &config);
    ~ServerSide();

    void setup();
};

