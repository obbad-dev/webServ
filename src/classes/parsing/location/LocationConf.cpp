#include "LocationConf.hpp"

#include <stdexcept>
#include <algorithm>
#include <cstdlib>
#include <cerrno>
#include <climits>

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
	hasClientMaxBodySizeFlag = false;
	clientMaxBodySize = 0;
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
	allowMethods.clear();
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
void LocationConf::setCgiPass(const std::string &extension, const std::string &ineterpreter)
{
	if (hasCgiPassFlag)
		throw std::invalid_argument("cgi_pass: duplicate directive in location block.");

	if (extension == ";" || ineterpreter == ";")
        throw std::invalid_argument("directive \"cgi_pass\" extension or interpreter has empty value.");

    if (extension != ".py" && extension != ".php" && extension != ".bla")
        throw std::invalid_argument("cgi_pass: my server only supports .py, .php, and .bla extensions for CGI scripts.");

    cgiPass = make_pair(extension, ineterpreter);
    hasCgiPassFlag = true;
}
void LocationConf::setClientMaxBodySize(const string &token)
{
	const size_t KB_MULTIPLIER = 1024ULL;
	const size_t MB_MULTIPLIER = 1024ULL * 1024ULL;
	const size_t GB_MULTIPLIER = 1024ULL * 1024ULL * 1024ULL;
	
    if (hasClientMaxBodySizeFlag)
        throw runtime_error("duplicate 'client_max_body_size' directive in location block");
    if (token == ";")
        throw invalid_argument("The 'client_max_body_size' directive is Empty");

    if (!token.empty() && token[0] == '-' && isdigit(token[1]))
        throw invalid_argument("number of client_max_body_size must be positive: " + token);
    char *end;
    errno = 0;
    unsigned long long value = strtoull(token.c_str(), &end, 10);
    if (end == token)
        throw invalid_argument("invalid number of client_max_body_size " + token);
    if (errno == ERANGE)
        throw invalid_argument("number client_max_body_size overflowed " + token);

    unsigned long long multiplier = 1;
    if (*end != '\0')
    {
        if (*(end + 1) != '\0')
            throw invalid_argument("Unsupported unit: " + token + " in client_max_body_size directive");

        char unit = *end;
        if (unit == 'M' || unit == 'm')
            multiplier = MB_MULTIPLIER;
        else if (unit == 'G' || unit == 'g')
            multiplier = GB_MULTIPLIER;
        else if (unit == 'K' || unit == 'k')
            multiplier = KB_MULTIPLIER;
        else
            throw invalid_argument("Unsupported unit: " + token + " in client_max_body_size directive");
    }
    if (value > ULLONG_MAX / multiplier)
            throw std::invalid_argument("'client_max_body_size' is too large: " + token);
    this->clientMaxBodySize = value * multiplier;
    this->hasClientMaxBodySizeFlag = true;
}

const bool &LocationConf::hasClientMaxBodySize() const
{
	return this->hasClientMaxBodySizeFlag;
}

const uint64_t &LocationConf::getClientMaxBodySize() const
{
	return this->clientMaxBodySize;
}

const pair<string, string> &LocationConf::getCgiPass() const
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