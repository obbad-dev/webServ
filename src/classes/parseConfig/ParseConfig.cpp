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

void ParseConfig::parseServer(Server &server, size_t &i)
{
    if (_tokens[incrIdx(i)] != "{")
        throw invalid_argument("Config file syntax error: expected '{' after 'server'.");

    while (i < _tokens.size())
    {
        string currentToken = _tokens[incrIdx(i)];

        if (currentToken == "listen"){
            server.setListen(_tokens[incrIdx(i)]);
        }
        else if (currentToken == "root")
        {
            server.setRoot(_tokens[incrIdx(i)]);
        }
        else if (currentToken == "}")
            break;
        else
            throw invalid_argument("Unsupported directive: '" + currentToken + "'.");
    
        if (_tokens[incrIdx(i)] != ";")
            throw invalid_argument("Syntax error: too many values in directive " + currentToken);
    }

    if (server.getListens().size() == 0 || server.getRoot().empty())
        throw invalid_argument("Config file must contain directives 'listen' and 'root'.");
}

size_t ParseConfig::incrIdx(size_t &i)
{
    if (i >= _tokens.size())
        throw invalid_argument("Block not completed.");
    return i++;
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
        string currentToken = _tokens[incrIdx(i)];

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
        cout << "Server [" << i << "]:   ";
        const vector<Listen> &listens = servers[i].getListens();
        for (size_t j = 0; j < listens.size(); j++)
            cout << "ip: " + listens[j].ip + ", port: " <<  listens[j].port << endl;
        cout << "Root: " << servers[i].getRoot() << endl;
    }
    cout << "----------------------------" << endl;
}
