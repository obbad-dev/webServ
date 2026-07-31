#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Server.hpp"

class ParseConfig
{
private:
    std::string _configFile;
    std::vector<std::string> _tokens;
    std::vector<Server> servers;

    void parseServer(Server &server, size_t &i);
    void tokenizeErrorPage(std::vector<std::string> &, size_t &);
    const std::vector<std::string> tokenizeMethods(size_t& i);
    const std::vector<std::string> tokenizeIndex(size_t& i);
    void parseLocations(Server &server, size_t &i);
    const std::string &consumeToken(size_t &i);

    void removeDuplicateListens();

public:
    ParseConfig(std::string configFile);
    ~ParseConfig();

    void tokenize();
    void parse();
    const std::vector<Server> &getServers() const;
};
