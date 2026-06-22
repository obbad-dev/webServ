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
    void tokenizeErrorPage(vector<string> &, size_t &);
    const vector<string> tokenizeMethods(size_t& i);
    const vector<string> tokenizeIndex(size_t& i);
    void parseLocations(Server &server, size_t &i);
    const string &consumeToken(size_t &i);

public:
    ParseConfig(string configFile);
    ~ParseConfig();

    void tokenize();
    void parse();
    void debug();
    const vector<Server> &getServers() const;
};
