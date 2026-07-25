#include "LocationConf.hpp"

#include <stdexcept>
#include <algorithm>
#include <cstdlib>
#include <cerrno>

LocationConf::LocationConf()
{
    allowMethods.insert("GET");
    allowMethods.insert("POST");
    allowMethods.insert("DELETE");
    setAllowMethodsFlag = false;
    autoindex = false;
    hasIndexFlag = false;
    setRootFlag = false;
    hasReturnFlag = false;
    uploadEnabled = false;
    hasUploadFlag = false;
	hasCgiPassFlag = false;
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
void LocationConf::setReturn(const string &path, const string &status)
{
    if (hasReturnFlag)
        throw invalid_argument("return: duplicate directive in location block.");
    if (path == ";" || status == ";" )
        throw invalid_argument("directive \"return\" has empty value.");

    char *endptr;
    errno = 0;
    long statusCode = strtol(status.c_str(), &endptr, 10);
    if (*endptr != '\0' || errno == ERANGE)
        throw invalid_argument("directive \"return\" has invalid status code.");
    if (statusCode !=301 && statusCode != 302)
        throw invalid_argument("directive \"return\" has invalid status code must be 301 or 302.");
    this->returnPair = make_pair(statusCode, path);
    hasReturnFlag = true;
}
void LocationConf::setUpload(const string &path)
{
    if (hasUploadFlag)
        throw invalid_argument("upload: duplicate directive in location block.");
    if (path == ";")
        throw invalid_argument("directive \"upload\" has empty value.");
    this->uploadPath = path;
    hasUploadFlag = true;
}
void LocationConf::setEnableUpload(const string &token)
{
    if (token == ";")
        throw invalid_argument("directive \"upload\" has empty value.");
    if (token == "on")
        this->uploadEnabled = true;
    else if (token == "off")
        this->uploadEnabled = false;
    else
        throw invalid_argument("Invalid value for upload: '" + token + "'. Valid values are 'on' or 'off'.");
}
void LocationConf::setCgiPass(const std::string &extension)
{
	if (hasCgiPassFlag)
		throw std::invalid_argument("cgi_pass: duplicate directive in location block.");

	if (extension == ";")
        throw std::invalid_argument("directive \"cgi_pass\" has empty value.");

    if (extension != ".py")
        throw std::invalid_argument("cgi_pass: my server only supports .py extension for CGI scripts.");

    cgiPass = extension;
    hasCgiPassFlag = true;
}

const string &LocationConf::getCgiPass() const
{
    return this->cgiPass;
}

const bool &LocationConf::uploadEnabledStatus() const
{
    return this->uploadEnabled;
}
const string &LocationConf::getUploadPath() const
{    return this->uploadPath;
}
const bool &LocationConf::uploadIsSet() const
{    return hasUploadFlag;
}
const pair<int, string> &LocationConf::getReturn() const
{
    return this->returnPair;
}
const bool &LocationConf::hasReturn() const
{    return hasReturnFlag;
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
const bool &LocationConf::rootIsSet() const
{
    return setRootFlag;
}
const bool &LocationConf::hasCgiPass() const
{
	return hasCgiPassFlag;
}

bool LocationConf::operator==(const LocationConf &other) const
{
    return (this->path == other.path);
}