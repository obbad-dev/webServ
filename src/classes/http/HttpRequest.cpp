#include "HttpRequest.hpp"
#include <stdexcept>
#include <cstring>
#include <sys/socket.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <algorithm>

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
    transform(key.begin(), key.end(), key.begin(), ::tolower);
    headers[key] = value;
}

void HttpRequest::setMethod(string method){
    if (method != "GET" && method != "POST" && method != "DELETE")
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
void HttpRequest::setBodyContent(string& body){
    this->bodyContent = body;
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

    if (getProtocolVersion() == "HTTP/1.1" && headers["host"] == "")
    {
        throw std::runtime_error("Missing Host header");
    }
    if (!headers["transfer-encoding"].empty())
    {
        string value = headers["transfer-encoding"];
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        if (value == "chunked")
           parseChunkedBody(clientFd, request);
        else 
            throw std::runtime_error("Unsupported Transfer-Encoding: " + value);
    }
    else if(!headers["content-length"].empty()){
        parseBody(clientFd, request);
    }
    debug();
}

void HttpRequest::parseChunkedBody(int &clientFd, string& request)
{
    string body;
    size_t pos = request.find("\r\n\r\n");
    request = request.substr(pos + 4);

    while (true)
    {
        if (request.find("\r\n0\r\n\r\n")  == string::npos)
        {
            char buffer[4096];
            memset(buffer, 0, sizeof(buffer));
            ssize_t byteRead = recv(clientFd, buffer, (sizeof(buffer) - 1), 0);
            if (byteRead <= 0)
            {
                perror("Error");
                throw std::runtime_error("Error reading request body");
            }
            request.append(buffer, static_cast<size_t>(byteRead));
        }
        size_t chunkSizeEnd = request.find("\r\n");
        if (chunkSizeEnd == string::npos)
            throw std::runtime_error("Invalid chunked body format");

        string chunkSizeStr = request.substr(0, chunkSizeEnd);
        char *end;
        errno = 0;
        long chunkSize = strtol(chunkSizeStr.c_str(), &end, 16);
        if (*end != '\0' || errno == ERANGE || chunkSize < 0)
            throw std::runtime_error("Invalid chunk size: " + chunkSizeStr);
        if (chunkSize == 0)
            break;
        size_t chunkStart = chunkSizeEnd + 2;
        string chunckData = request.substr(chunkStart, chunkSize);
        if (chunckData.find("\r\n") != string::npos)
            throw runtime_error("body chunck error");
        body.append(chunckData);
        request.erase(0, chunkSizeEnd + chunkSize + 2);
    }
    setBodyContent(body);
}

void HttpRequest::parseBody(int &clientFd, string& request)
{
    char *endPtr;
    errno = 0;
    long contentLength = strtol(headers["content-length"].c_str(), &endPtr, 10);
    if (*endPtr != '\0' || errno == ERANGE || contentLength < 0)
        throw std::runtime_error("Invalid Content-Length header");
    string body;
    body = request.substr(request.find("\r\n\r\n") + 4);
    if (static_cast<long>(body.size()) < contentLength)
    {
        while (static_cast<long>(body.size()) < contentLength)
        {
            char buffer[4096];
            memset(buffer, 0, sizeof(buffer));
            ssize_t byteRead = recv(clientFd, buffer, (sizeof(buffer) - 1), 0);
            if (byteRead <= 0)
            {
                perror("Error");
                throw std::runtime_error("Error reading request body");
            }
            body.append(buffer, static_cast<size_t>(byteRead));
        }
    }
    if (static_cast<long>(body.size()) > contentLength)
        body.resize(contentLength);

    setBodyContent(body);
}

void HttpRequest::debug()
{
    cout << "Method: " << this->method << '\n';
    cout << "Target: " << this->target << '\n';
    cout << "Protocol: " << this->protocolVersion << '\n';
    cout << "------------Headers-----------" << '\n';
    for (map<string, string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
        cout << it->first << ": " << it->second << '\n';
    cout << "------------Body-----------" << '\n';
    cout << this->bodyContent << '\n';
    cout << "------------------------------------" << '\n';
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

void HttpRequest::create_response(int &clientFd)
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
