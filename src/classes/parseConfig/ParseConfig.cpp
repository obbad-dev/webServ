#include "ParseConfig.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stack>
#include <stdexcept>

ParseConfig::ParseConfig(std::string configFile) : _configFile(configFile)
{
    tokenize();
    parse();
    debug();
}

ParseConfig::~ParseConfig()
{
}

void ParseConfig::parseLocations(Server &server, size_t &i)
{
    LocationConf locationConf;
    locationConf.setPath(consumeToken(i));
    bool needsSemicolon = true;
    if (consumeToken(i) != "{")
        throw invalid_argument("directive \"location\" has no opening \"{\"");
    while (i < _tokens.size())
    {
        const string &currToken = consumeToken(i);
        if (currToken == "allow_methods")
        {
            locationConf.setAllowMethods(tokenizeMethods(i));
            needsSemicolon = false;
        }
        else if (currToken == "}")
            break;
        else
            throw invalid_argument("unexpected token '" + currToken + "'");
        if (needsSemicolon && consumeToken(i) != ";")
            throw invalid_argument("Syntax error: too many values in directive " + currToken);
    }
    server.pushLocation(locationConf);
}

void ParseConfig::parseServer(Server &server, size_t &i)
{
    if (consumeToken(i) != "{")
        throw invalid_argument("Config file syntax error: expected '{' after 'server'.");

    while (i < _tokens.size())
    {
        const string &currentToken = consumeToken(i);

        bool needsSemicolon = true;

        if (currentToken == "}")
            break;
        else if (currentToken == "listen")
            server.setListen(consumeToken(i));
        else if (currentToken == "root")
            server.setRoot(consumeToken(i));
        else if (currentToken == "index")
            server.setIndex(consumeToken(i));
        else if (currentToken == "client_max_body_size")
            server.setClientMaxBodySize(consumeToken(i));
        else if (currentToken == "error_page"){
            vector<string> errorTokens;
            tokenizeErrorPage(errorTokens, i);
            server.setErrorsPages(errorTokens);
            needsSemicolon = false;
        }
        else if (currentToken == "location"){
            parseLocations(server, i);
            needsSemicolon = false;
        }
        else
            throw invalid_argument("Unsupported directive: '" + currentToken + "'.");

        if (needsSemicolon && consumeToken(i) != ";")
            throw invalid_argument("Syntax error: too many values in directive " + currentToken);
    }
    if (server.getListens().size() == 0 || server.getRoot().empty())
        throw invalid_argument("Config file must contain directives 'listen' and 'root'.");
}

void ParseConfig::parse()
{
    size_t i = 0;

    if (_tokens.empty())
        throw runtime_error("File is empty: must contain [server] block with [listen, root].");

    while (i < _tokens.size())
    {
        Server server;
        const string &currentToken = consumeToken(i);

        if (currentToken == "server")
            parseServer(server, i);
        else
            throw invalid_argument("Config file must start with [server], not " + currentToken + ".");

        servers.push_back(server);
    }
}

const vector<Server> &ParseConfig::getSrvers() const
{
    return servers;
}

void ParseConfig::debug()
{
    cout << "--- Parsed Configuration ---" << endl;
    for (size_t i = 0; i < servers.size(); ++i)
    {
        cout << "Server [" << i << "]:" << endl;
        const vector<Listen> &listens = servers[i].getListens();
        for (size_t j = 0; j < listens.size(); j++)
            cout << "ip: " + listens[j].ip + ", port: " << listens[j].port << endl;
        cout << "Root: " << servers[i].getRoot() << endl;
        cout << "index: " << servers[i].getIndex() << endl;
        cout << "client max body size: " << servers[i].getClientMaxBodySize() << "B" << endl;
        string uri;
        cout << "error_page: ";
        for (std::map<int, std::string>::const_iterator it = servers[i].getErrorsPages().begin();
             it != servers[i].getErrorsPages().end(); it++)
        {
            uri = it->second;
            cout << it->first << " ";
        }
        cout << uri << endl;

        cout << "Locations:" << endl;
        const vector<LocationConf> &locations = servers[i].getLocations();
        for (size_t j = 0; j < locations.size(); ++j)
        {
            cout << "  - " << locations[j].getPath() << endl;
            cout << "    allow_methods: ";
            const set<string> &methods = locations[j].getAllowMethods();
            for (set<string>::const_iterator it = methods.begin(); it != methods.end(); ++it)
                cout << *it << " ";
            cout << endl;
            
        }
    }
    cout << "----------------------------" << endl;
}
