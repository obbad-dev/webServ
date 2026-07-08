#include "HttpRequest.hpp"
#include <stdexcept>
#include <cstring>
#include <sys/socket.h>
#include <sstream>
#include <iostream>
#include <fstream>
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
void HttpRequest::parseRequest(int clientFd){
    string request = readRequest(clientFd);
    // std::cout << request << std::endl;
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

            setMethod(method); // why setters when you can set them directly
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

    while (req.find("\r\n\r\n") == string::npos)
    {
        memset(buffer, 0, sizeof(buffer));
        ssize_t byteRead = recv(clientFd, buffer, (sizeof(buffer) - 1), 0);
        if (byteRead < 0)
        {
            perror("Error");
            throw runtime_error("");
        }
        req.append(buffer, static_cast<size_t>(byteRead));
    }
    return req;
}

bool read_content(string &content, string &path)
{
    ifstream file(path.c_str());
    if (!file.is_open())
        return false;

    string tmp_content;
    while (1)
    {
        getline(file, tmp_content);
        content += tmp_content;
        if (file.eof())
            break;
        content += "\n";
    }
    return true;
}

string conv_to_str(int number)
{
    ostringstream s;
    s << number;
    return s.str();
}

string retrieve_extension(map<string, string> &extensions, string &path)
{
    string substring;
    size_t pos = path.rfind(".");
    if (pos != string::npos)
        substring = path.substr(pos);
    else
        return "text/plain";

    map<string, string>::iterator it = extensions.find(substring);
    if (it != extensions.end())
        return it->second;
    else
        return "text/plain";
}

void HttpRequest::create_response(int clientFd)
{
    string response;
    if (method == "GET")
    {
        string path;
        if (target == "/")
            path = "./resources/sites/index.html";
        else
            path = "./resources/sites" + target;

        string content;
        if (read_content(content, path))
        {
            response += "HTTP/1.1 200 OK\r\n";
            response += "Content-Type: ";
            response += retrieve_extension(extensions, path);
            response += "\r\n";
            response += "Content-Length: ";
            response += conv_to_str(content.size());
            response += "\r\n";
            response += "\r\n";
            response += content;
        }
        else
        {
            response += "HTTP/1.1 404 Not Found\r\n";
            response += "Content-Type: text/plain\r\n";
            response += "Content-Length: 9\r\n";
            response += "\r\n";
            response += "Not Found";
        }
        if (send(clientFd, response.c_str(), response.size(), 0) == -1)
        {
            perror("send");
            return;
        }
    }
    // else if (method == "POST")
    // {

    // }
    // else if (method = "DELETE")
    // {

    // }
    // else
    // {
    //     // send ERROR;
    // }
}
