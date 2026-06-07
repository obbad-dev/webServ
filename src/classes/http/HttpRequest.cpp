#include "HttpRequest.hpp"

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
    headers[key] = value;
}
void HttpRequest::setMethod(string method){
    this->method = method;
}
void HttpRequest::setTarget(string target){
    this->target = target;
}
void HttpRequest::setProtocolVersion(string version)
{
    protocolVersion = version;
}