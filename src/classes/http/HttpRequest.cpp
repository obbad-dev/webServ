#include "HttpRequest.hpp"

HttpRequest::HttpRequest(): method(""), path("") {}
HttpRequest::~HttpRequest() {}

const map<string, string>& HttpRequest::getHeaders() const
{
    return this->headers;
}

const string& HttpRequest::getMethod() const {
    return this->method;
}

const string& HttpRequest::getPath() const {
    return this->path;
}