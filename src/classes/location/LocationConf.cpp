#include "LocationConf.hpp"

#include <stdexcept>
#include <algorithm>

LocationConf::LocationConf()
{
    allowMethods.insert("GET");
    allowMethods.insert("POST");
    allowMethods.insert("DELETE");
    setAllowMethodsFlag = false;
}

LocationConf::~LocationConf()
{
}

void LocationConf::setPath(const string &token)
{
    if (token == "{" || token == "}")
        throw invalid_argument("invalid number of arguments in \"location\" directive ");
    if (token == ";")
        throw invalid_argument("directive \"location\" has no opening \"{\"");
    this->path = token;
}
void LocationConf::setAllowMethods(const vector<string> &tokens)
{
    if (setAllowMethodsFlag)
        throw invalid_argument("allow_methods: duplicate directive.");
    vector<string> validMethods;
    validMethods.push_back("GET");
    validMethods.push_back("POST");
    validMethods.push_back("DELETE");   
    for (size_t i = 0; i < tokens.size(); ++i)
    {
        if (find(validMethods.begin(), validMethods.end(), tokens[i]) == validMethods.end())
            throw invalid_argument("Invalid method: '" + tokens[i] + "'. Valid methods are GET, POST, DELETE.");
         this->allowMethods.insert(tokens[i]);
    }
    setAllowMethodsFlag = true;  
}

const set<string> &LocationConf::getAllowMethods() const
{
    return this->allowMethods;
}

const string &LocationConf::getPath() const
{
    return this->path;
}