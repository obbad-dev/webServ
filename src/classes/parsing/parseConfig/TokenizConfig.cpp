#include "ParseConfig.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <algorithm>

void ParseConfig::tokenizeErrorPage(std::vector<std::string> &errorTokens, size_t &i)
{
    while (i < _tokens.size())
    {
        const std::string &token = consumeToken(i);
        if (token == ";")
            break;
        if (token == "{" || token == "}")
            throw std::invalid_argument("error_page: expected end with ';'");
        errorTokens.push_back(token);
    }
    if (errorTokens.size() < 2)
        throw std::invalid_argument("error_page: missing status code or URI");
}

void ParseConfig::tokenize()
{
    std::ifstream file(_configFile.c_str());
    if (!file.is_open())
        throw std::runtime_error("Error: Could not open config file: " + _configFile);

    std::string line;
    std::stack<std::string> bracesCheck;

    while (std::getline(file, line))
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

        std::istringstream iss(line);
        std::string word;

        while (iss >> word)
        {
            if (word == "{" || word == "}")
            {
                if (word == "{")
                    bracesCheck.push(word);
                else if (!bracesCheck.empty() && word == "}" && bracesCheck.top() == "{")
                    bracesCheck.pop();
                else
                    throw std::runtime_error("Error: Unmatched closing brace '}'");
            }
            _tokens.push_back(word);
        }
    }

    if (!bracesCheck.empty())
        throw std::runtime_error("Error: Unclosed brace '{'");
}

const std::vector <std::string> ParseConfig::tokenizeMethods(size_t &i)
{
    std::vector<std::string> methods;

    while (i < _tokens.size())
    {
        const std::string &token = consumeToken(i);
        if (token == ";")
            break;
        else if (token == "{" || token == "}")
            throw std::invalid_argument("allow_methods: expected end with ';'");
        methods.push_back(token);
    }
    if (methods.empty())
        throw std::invalid_argument("allow_methods: no methods specified.");

    return methods;
}
const std::vector <std::string> ParseConfig::tokenizeIndex(size_t &i)
{
    std::vector<std::string> indexFiles;

    while (i < _tokens.size())
    {
        const std::string &token = consumeToken(i);
        if (token == ";")
            break;
        else if (token == "{" || token == "}")
            throw std::invalid_argument("index: expected end with ';'");
        indexFiles.push_back(token);
    }
    if (indexFiles.empty())
        throw std::invalid_argument("index: no index files specified.");

    return indexFiles;
}

const std::string &ParseConfig::consumeToken(size_t &i)
{
    if (i >= _tokens.size())
        throw std::invalid_argument("Block not completed.");
    return _tokens[i++];
}
