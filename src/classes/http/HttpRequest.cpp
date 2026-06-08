#include "HttpRequest.hpp"
#include <stdexcept>

HttpRequest::HttpRequest(): method(""), target("") {}
HttpRequest::~HttpRequest() {}

const map<string, string>& HttpRequest::getHeaders() const
{
    return this->headers;
}
const string& HttpRequest::getMethod() const {
    return this->method;
}
const string& HttpRequest::getPath() const {
    return this->target;
}
const string& HttpRequest::getProtocolVersion() const{
    return this->protocolVersion;
}

void HttpRequest::setHeaders(string key, string value){
    while (!value.empty() && (value[0] == ' ' || value[0] == '\t'))
    {
        value.erase(0, 1);
    }
    headers[key] = value;
}
void HttpRequest::setMethod(string method){
    if (method != "GET")
        throw std::runtime_error("Unsupported method");
    this->method = method;
}
void HttpRequest::setTarget(string target){
    this->target = target;
}
void HttpRequest::setProtocolVersion(string version)
{
    if (version != "HTTP/1.1" && version != "HTTP/1.0")
        throw std::runtime_error("Unsupported HTTP version");
    protocolVersion = version;
}