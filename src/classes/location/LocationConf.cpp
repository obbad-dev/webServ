#include "LocationConf.hpp"

#include <stdexcept>
#include <algorithm>

LocationConf::LocationConf()
{
    allowMethods.insert("GET");
    allowMethods.insert("POST");
    allowMethods.insert("DELETE");
    setAllowMethodsFlag = false;
    autoindex = false;
    hasIndexFlag = false;
    setRootFlag = false;
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
void LocationConf::setRoot(const string &rootPath)
{
    if (setRootFlag)
        throw invalid_argument("root: duplicate directive in location block.");
    if (rootPath == ";")
        throw invalid_argument("root: empty root path.");
    this->root = rootPath;
    this->setRootFlag = true;
}
void LocationConf::setAutoindex(const string &token)
{
    if (autoindex)
        throw invalid_argument("autoindex: duplicate directive in location block.");
    if (token == ";")
        throw invalid_argument("autoindex: empty value."); 
    if (token == "on")
        this->autoindex = true;
    else if (token == "off")
        this->autoindex = false;
    else
        throw invalid_argument("Invalid value for autoindex: '" + token + "'. Valid values are 'on' or 'off'.");
}
void LocationConf::setIndex(const vector<string> &indexFiles)
{
    if (indexFiles.empty())
        throw invalid_argument("index: empty value.");
    this->index.insert(this->index.end(), indexFiles.begin(), indexFiles.end());
    this->hasIndexFlag = true;
}

const bool &LocationConf::hasAutoindex() const
{
    return this->autoindex;
}
const string &LocationConf::getRoot() const
{
    return this->root;
}
const set<string> &LocationConf::getAllowMethods() const
{
    return this->allowMethods;
}

const string &LocationConf::getPath() const
{
    return this->path;
}
const vector<string> &LocationConf::getIndex() const
{
    return this->index;
} 
const bool &LocationConf::indexIsSet() const
{
    return hasIndexFlag;
}