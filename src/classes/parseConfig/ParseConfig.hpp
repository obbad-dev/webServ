#pragma once

using namespace std;
#include <cstddef>
#include <string>
#include <vector>

#include "Server.hpp"

class ParseConfig
{
private:
    string _configFile;
    vector<string> _tokens;
    vector<Server> servers;

    void parseServer(Server &server, size_t &i);
    size_t incrIdx(size_t &i);

public:
    ParseConfig(string configFile);
    ~ParseConfig();

    void tokenize();
    void parse();
    const vector<Server> &getSrvers() const;
    void debug();
};
