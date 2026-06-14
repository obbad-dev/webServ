#include "LocationConf.hpp"

#include <stdexcept>

LocationConf::LocationConf()
{
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

const string &LocationConf::getPath() const
{
    return this->path;
}