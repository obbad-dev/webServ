#include "ParseConfig.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <algorithm>

ParseConfig::ParseConfig(std::string configFile) : _configFile(configFile)
{
    tokenize();
    parse();
    
}
ParseConfig::~ParseConfig()
{
}

void ParseConfig::parseLocations(Server &server, size_t &i)
{
    LocationConf locationConf;
    locationConf.setPath(consumeToken(i));
    if (consumeToken(i) != "{")
        throw std::invalid_argument("directive \"location\" has no opening \"{\"");
    while (i < _tokens.size())
    {
        bool needsSemicolon = true;
        const std::string &currToken = consumeToken(i);
        if (currToken == "allow_methods")
        {
            locationConf.setAllowMethods(tokenizeMethods(i));
            needsSemicolon = false;
        }
        else if (currToken == "root")
            locationConf.setRoot(consumeToken(i));
        else if (currToken == "autoindex")
            locationConf.setAutoindex(consumeToken(i));
        else if (currToken == "index")
        {
            locationConf.setIndex(tokenizeIndex(i));
            needsSemicolon = false;
        }
        else if (currToken == "return")
        {
            std::string status = consumeToken(i);
            std::string path = consumeToken(i);
            locationConf.setReturn(path, status);
        }
        else if (currToken == "upload_path")
            locationConf.setUpload(consumeToken(i));
        else if (currToken == "enable_upload")
            locationConf.setEnableUpload(consumeToken(i));
		else if (currToken == "client_max_body_size")
            locationConf.setClientMaxBodySize(consumeToken(i));
        else if (currToken == "cgi_pass")
        {
            const std::string& extension = consumeToken(i);
           const std::string& interpreter = consumeToken(i);
            locationConf.setCgiPass(extension, interpreter);
        }
        else if (currToken == "}")
            break;
        else
            throw std::invalid_argument("unexpected token '" + currToken + "'");
        if (needsSemicolon && consumeToken(i) != ";")
            throw std::invalid_argument("Syntax error: too many values in directive " + currToken);
    }
    if (locationConf.uploadEnabledStatus() && !locationConf.uploadIsSet())
        throw std::invalid_argument("directive \"upload_path\": 'enable_upload' is set to 'on' but no upload path is specified.");
    if (std::find(server.getLocations().begin(), server.getLocations().end(), locationConf) != server.getLocations().end())
        throw std::invalid_argument("duplicate location block for path: " + locationConf.getPath());
    server.pushLocation(locationConf);
}

void ParseConfig::parseServer(Server &server, size_t &i)
{
    if (consumeToken(i) != "{")
        throw std::invalid_argument("Config file syntax error: expected '{' after 'server'.");

    while (i < _tokens.size())
    {
        const std::string &currentToken = consumeToken(i);

        bool needsSemicolon = true;

        if (currentToken == "}")
            break;
        else if (currentToken == "listen")
            server.setListen(consumeToken(i));
        else if (currentToken == "root")
            server.setRoot(consumeToken(i));
        else if (currentToken == "index")
        {
            server.setIndex(tokenizeIndex(i));
            needsSemicolon = false;
        }
        else if (currentToken == "client_max_body_size")
            server.setClientMaxBodySize(consumeToken(i));
        else if (currentToken == "error_page"){
            std::vector<std::string> errorTokens;
            tokenizeErrorPage(errorTokens, i);
            server.setErrorsPages(errorTokens);
            needsSemicolon = false;
        }
        else if (currentToken == "location"){
            parseLocations(server, i);
            needsSemicolon = false;
        }
        else
            throw std::invalid_argument("Unsupported directive: '" + currentToken + "'.");

        if (needsSemicolon && consumeToken(i) != ";")
            throw std::invalid_argument("Syntax error: too many values in directive " + currentToken);
    }
    if (server.getListens().size() == 0 || server.getRoot().empty())
        throw std::invalid_argument("Config file must contain directives 'listen' and 'root'.");
}

void ParseConfig::parse()
{
    size_t i = 0;

    if (_tokens.empty())
        throw std::runtime_error("File is empty: must contain [server] block with [listen, root].");

    while (i < _tokens.size())
    {
        Server server;
        const std::string &currentToken = consumeToken(i);

        if (currentToken == "server")
            parseServer(server, i);
        else
            throw std::invalid_argument("Config file must start with [server], not " + currentToken + ".");

        std::vector<LocationConf> &locations = server.getForModifyLocation();
        for (size_t k = 0; k < locations.size(); ++k)
        {
            if (!locations[k].rootIsSet())
                locations[k].setRoot(server.getRoot());
            if (!locations[k].indexIsSet())
                locations[k].setIndex(server.getIndex());
        }
        servers.push_back(server);
    }
    removeDuplicateListens();
}

const std::vector<Server> &ParseConfig::getServers() const
{
    return servers;
}

void ParseConfig::removeDuplicateListens()
{
    for (size_t i = 0; i < servers.size(); i++)
    {
        const std::vector<Listen> &listens_i = servers[i].getListens();

        for (size_t j = i + 1; j < servers.size(); )
        {
            const std::vector<Listen> &listens_j = servers[j].getListens();
            bool duplicate = false;

            for (size_t a = 0; a < listens_i.size() && !duplicate; a++)
            {
                for (size_t b = 0; b < listens_j.size() && !duplicate; b++)
                {
                    if (listens_i[a] == listens_j[b])
                        duplicate = true;
                }
            }
            if (duplicate)
                throw std::invalid_argument("Duplicate listen directive in two different server blocks" );
            else
                j++;
        }
    }
}
