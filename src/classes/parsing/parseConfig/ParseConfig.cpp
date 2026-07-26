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
    // cout << "Configuration file parsed successfully." << endl;
}
ParseConfig::~ParseConfig()
{
}

void ParseConfig::parseLocations(Server &server, size_t &i)
{
    LocationConf locationConf;
    locationConf.setPath(consumeToken(i));
    if (consumeToken(i) != "{")
        throw invalid_argument("directive \"location\" has no opening \"{\"");
    while (i < _tokens.size())
    {
        bool needsSemicolon = true;
        const string &currToken = consumeToken(i);
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
            string status = consumeToken(i);
            string path = consumeToken(i);
            locationConf.setReturn(path, status);
        }
        else if (currToken == "upload_path")
            locationConf.setUpload(consumeToken(i));
        else if (currToken == "enable_upload")
            locationConf.setEnableUpload(consumeToken(i));
        else if (currToken == "cgi_pass")
        {
            string extension = consumeToken(i);
            locationConf.setCgiPass(extension);
        }
        else if (currToken == "}")
            break;
        else
            throw invalid_argument("unexpected token '" + currToken + "'");
        if (needsSemicolon && consumeToken(i) != ";")
            throw invalid_argument("Syntax error: too many values in directive " + currToken);
    }
    if (locationConf.uploadEnabledStatus() && !locationConf.uploadIsSet())
        throw invalid_argument("directive \"upload_path\": 'enable_upload' is set to 'on' but no upload path is specified.");
    if (find(server.getLocations().begin(), server.getLocations().end(), locationConf) != server.getLocations().end())
        throw invalid_argument("duplicate location block for path: " + locationConf.getPath());
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
        {
            server.setIndex(tokenizeIndex(i));
            needsSemicolon = false;
        }
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
        else if (currentToken == "server_name")
            server.setServerName(consumeToken(i));
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

        vector<LocationConf> &locations = server.getForModifyLocation();
        for (size_t k = 0; k < locations.size(); ++k)
        {
            if (!locations[k].rootIsSet())
                locations[k].setRoot(server.getRoot());
            if (!locations[k].indexIsSet())
                locations[k].setIndex(server.getIndex());
        }
        servers.push_back(server);
    }
}

const vector<Server> &ParseConfig::getServers() const
{
    return servers;
}

// void ParseConfig::debug()
// {
//     cout << "--- Parsed Configuration ---" << endl;
//     for (size_t i = 0; i < servers.size(); ++i)
//     {
//         cout << "Server [" << i << "]:   ";
//         const vector<Listen> &listens = servers[i].getListens();
//         for (size_t j = 0; j < listens.size(); j++)
//             cout << "ip: " + listens[j].ip + ", port: " << listens[j].port << endl;
//         cout << "Root: " << servers[i].getRoot() << endl;
//         cout << "index: ";
//         for (size_t j = 0; j < servers[i].getIndex().size(); ++j)
//         {
//             cout << servers[i].getIndex()[j] << " ";
//         }
//         cout << endl;
//         cout << "client max body size: " << servers[i].getClientMaxBodySize() << "B" << endl;
//         string uri;
//         cout << "error_page: ";
//         for (std::map<int, std::string>::const_iterator it = servers[i].getErrorsPages().begin();
//              it != servers[i].getErrorsPages().end(); it++)
//         {
//             uri = it->second;
//             cout << it->first << " ";
//         }
//         cout << uri << endl;
//         cout << "server name: " << servers[i].getServerName() << endl;

//         cout << "Locations:" << endl;
//         const vector<LocationConf> &locations = servers[i].getLocations();
//         for (size_t j = 0; j < locations.size(); ++j)
//         {
//             cout << "  - " << locations[j].getPath() << endl;
//             cout << "    allow_methods: ";
//             const set<string> &methods = locations[j].getAllowMethods();
//             for (set<string>::const_iterator it = methods.begin(); it != methods.end(); ++it)
//                 cout << *it << " ";
//             cout << endl;
//             cout << "    root: " << locations[j].getRoot() << endl;
            
//             cout << "    index: " ;
//             for (size_t k = 0; k < locations[j].getIndex().size(); ++k)
//             {
//                 cout << locations[j].getIndex()[k] << " ";
//             }
//             cout << endl;
//             cout << "    autoindex: " << (locations[j].hasAutoindex() ? "on" : "off") << endl;
//             if (locations[j].getReturn().first != 0)
//                 cout << "    return: " << locations[j].getReturn().first << " " << locations[j].getReturn().second << endl;
//             cout << "    upload_enabled: " << (locations[j].uploadEnabledStatus() ? "on" : "off") << endl;
//             if (locations[j].uploadEnabledStatus())
//                 cout << "    upload_path: " << locations[j].getUploadPath() << endl;

//             const map<string, string> &cgiPassMap = locations[j].getCgiPass();
//             if (!cgiPassMap.empty()){
//                 for (map<string, string>::const_iterator it = cgiPassMap.begin(); it != cgiPassMap.end(); ++it)
//                 {
//                     cout << "    cgi_pass: ";
//                     cout  << it->first << " => " << it->second << endl;
//                 }  
//             }
//         }
//     cout << "----------------------------" << endl;
// }
// }