#include "ParseConfig.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <algorithm>

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

const vector <string> ParseConfig::tokenizeMethods(size_t &i)
{
    vector<string> methods;

    while (i < _tokens.size())
    {
        const string &token = consumeToken(i);
        if (token == ";")
            break;
        else if (token == "{" || token == "}")
            throw invalid_argument("allow_methods: expected end with ';'");
        methods.push_back(token);
    }
    if (methods.empty())
        throw invalid_argument("allow_methods: no methods specified.");

    return methods;
}

const string &ParseConfig::consumeToken(size_t &i)
{
    if (i >= _tokens.size())
        throw invalid_argument("Block not completed.");
    return _tokens[i++];
}
