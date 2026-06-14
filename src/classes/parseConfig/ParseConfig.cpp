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

void ParseConfig::tokenizeErrorPage(vector<string> &errorTokens, size_t &i)
{
    while (i < _tokens.size())
    {
        const string &token = consumeToken(i);
        if (token == ";")
            break;
        if (token == "{" || token == "}")
            throw invalid_argument("error_page: expected end with ';'");
        errorTokens.push_back(token);
    }
    if (errorTokens.size() < 2)
        throw invalid_argument("error_page: missing status code or URI");
}
void ParseConfig::parseLocations(Server& server, size_t& i)
{
    LocationConf locationConf;
    locationConf.setPath(consumeToken(i));
    if (consumeToken(i) != "{")
        throw invalid_argument("directive \"location\" has no opening \"{\"");
    while (i < _tokens.size())
    {
        const string& currToken = consumeToken(i);
        if (currToken == "}")
            break;
        else
            throw invalid_argument("unexpected token '"+currToken+"'");
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

        if (currentToken == "listen")
        {
            server.setListen(consumeToken(i));
        }
        else if (currentToken == "root")
        {
            server.setRoot(consumeToken(i));
        }
        else if (currentToken == "index")
        {
            server.setIndex(consumeToken(i));
        }
        else if (currentToken == "client_max_body_size")
            server.setClientMaxBodySize(consumeToken(i));
        else if (currentToken == "error_page")
        {
            vector<string> errorTokens;
            tokenizeErrorPage(errorTokens, i);
            server.setErrorsPages(errorTokens);
            needsSemicolon = false;
        }
        else if (currentToken == "location")
        {
            parseLocations(server, i);
            needsSemicolon = false;
        }
        else if (currentToken == "}")
            break;
        else
            throw invalid_argument("Unsupported directive: '" + currentToken + "'.");

        if (needsSemicolon && consumeToken(i) != ";")
        {
            throw invalid_argument("Syntax error: too many values in directive " + currentToken);
        }
    }

    if (server.getListens().size() == 0 || server.getRoot().empty())
        throw invalid_argument("Config file must contain directives 'listen' and 'root'.");
}

const string &ParseConfig::consumeToken(size_t &i)
{
    if (i >= _tokens.size())
        throw invalid_argument("Block not completed.");
    return _tokens[i++];
}

void ParseConfig::tokenize()
{
    ifstream file(_configFile.c_str());
    if (!file.is_open())
        throw runtime_error("Error: Could not open config file: " + _configFile);

    string line;
    stack<std::string> bracesCheck;

    while (getline(file, line))
    {
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos)
            line = line.substr(0, commentPos);

        if (line.empty())
            continue;

        for (size_t i = 0; i < line.length(); ++i)
        {
            if (line[i] == '{' || line[i] == '}' || line[i] == ';')
            {
                line.insert(i + 1, " ");
                line.insert(i, " ");
                i += 2;
            }
        }

        istringstream iss(line);
        string word;

        while (iss >> word)
        {
            if (word == "{" || word == "}")
            {
                if (word == "{")
                    bracesCheck.push(word);
                else if (!bracesCheck.empty() && word == "}" && bracesCheck.top() == "{")
                    bracesCheck.pop();
                else
                    throw runtime_error("Error: Unmatched closing brace '}'");
            }
            _tokens.push_back(word);
        }
    }

    if (!bracesCheck.empty())
        throw runtime_error("Error: Unclosed brace '{'");
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
            cout << "  - " << locations[j].getPath() << endl;
    }
    cout << "----------------------------" << endl;
}
