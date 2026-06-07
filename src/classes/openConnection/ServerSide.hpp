#pragma once
using namespace std;

#include <string>

#include "ParseConfig.hpp"

class ServerSide
{
private:
    ParseConfig &_config;

    string readRequest(int &clientFd);
    string parseRequest(string& buffer);

public:
    ServerSide(ParseConfig &config);
    ~ServerSide();

    void setup();
};

