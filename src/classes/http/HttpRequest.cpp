#include "HttpRequest.hpp"
#include <stdexcept>
#include <cstring>
#include <sys/socket.h>
#include <sstream>
#include <cstdio>


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


// parse request
void HttpRequest::parseRequest(int& clientFd){
    string request = readRequest(clientFd);
    
    if (request.find("\r\n\r\n") == std::string::npos)
        throw std::runtime_error("Incomplete HTTP request");

    size_t start = 0;
    size_t end;

    bool requestLineParsed = false;

    while ((end = request.find("\r\n", start)) != std::string::npos)
    {
        std::string line = request.substr(start, end - start);

        if (line.empty())
            break;

        if (!requestLineParsed)
        {
            std::istringstream iss(line);

            std::string method;
            std::string target;
            std::string version;
            std::string extra;

            if (!(iss >> method >> target >> version))
                throw std::runtime_error("Invalid request line");

            if (iss >> extra)
                throw std::runtime_error("Too many fields in request line");

            setMethod(method);
            setTarget(target);
            setProtocolVersion(version);
            requestLineParsed = true;
        }
        else
        {
            size_t pos = line.find(':');

            if (pos == std::string::npos)
                throw std::runtime_error("Invalid header: " + line);

            if (pos == 0)
                throw std::runtime_error("Empty header name");

            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            setHeaders(key, value);
        }
        start = end + 2;
    }

    if (getProtocolVersion() == "HTTP/1.1" && headers["Host"] == "")
    {
        throw std::runtime_error("Missing Host header");
    }
    // debug();
}

// parse request
string HttpRequest::readRequest(int& clientFd){
    string req;
    char buffer[4096];

    while (true)
    {
        memset(buffer, 0, sizeof(buffer));
        ssize_t byteRead = recv(clientFd, buffer, sizeof(buffer), 0);

        if (byteRead < 0)
        {
            perror("Error");
            throw runtime_error("");
        }
        if (byteRead == 0)
            break;
        req.append(buffer, static_cast<size_t>(byteRead));
    }

    return req;
}

