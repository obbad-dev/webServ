#include "HttpRequest.hpp"
#include <stdexcept>
#include <cstring>
#include <sys/socket.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <algorithm>
#include <unistd.h>

HttpRequest::HttpRequest(): method(""), path("") {
    headers_parsed = false;
    contentLength = 0;
    expectedChunkSize = 0;
    chunk_state = READ_SIZE;
    is_complete = false;
    debuging = false;
}
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
const string& HttpRequest::getProtocolVersion() const{
    return this->protocolVersion;
}

void HttpRequest::setHeaders(string key, string value){
    while (value[0] == ' ' || value[0] == '\t')
    {
        value.erase(0, 1);
    }
    transform(key.begin(), key.end(), key.begin(), ::tolower);
    headers[key] = value;
}
void HttpRequest::setMethod(string method){
    if (method != "GET" && method != "POST" && method != "DELETE")
        throw std::runtime_error(std::string(ERR_UNSUPPORTED_METHOD) + method);
    this->method = method;
}
void HttpRequest::setTarget(string target){
    if (target.empty())
        throw std::runtime_error("Invalid target: " + target);
    this->path = target;
}
void HttpRequest::setProtocolVersion(string version)
{
    if (version.empty())
        throw std::runtime_error(std::string(ERR_INVALID_PROTOCOL) + version);
    if (version != "HTTP/1.1" && version != "HTTP/1.0")
        throw std::runtime_error(std::string(ERR_UNSUPPORTED_VERSION) + version);
    protocolVersion = version;
}

void HttpRequest::setBodyType()
{
    if (headers.find("transfer-encoding") != headers.end() && headers["transfer-encoding"] == "chunked"){
        body_type = CHUNKED;
    }
    else if (headers.find("content-length") != headers.end()){
        body_type = CONTENT_LENGTH;
        char *endPtr;
        errno = 0;
        long number = strtol(headers["content-length"].c_str(), &endPtr, 10);
        if (*endPtr != '\0' || errno == ERANGE || number < 0)
            throw std::runtime_error(ERR_INVALID_CONTENT_LEN);
        contentLength = number;
    }
    else{
        body_type = NONE;
    }
}

//read Request
string HttpRequest::readRequest(int& clientFd)
{
    char buffer[4096];
    errno = 0;
    memset(buffer, 0, sizeof(buffer));
    ssize_t byteRead = recv(clientFd, buffer, (sizeof(buffer) - 1), 0);
    if (byteRead == 0)
    {
        cout  << "READ REquest " << endl;
        return "";
    }
    if (byteRead < 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK ){
            throw std::runtime_error(ERR_READ);
        }
    }
    return buffer;
}

void HttpRequest::parseHeaders(string& buffer)
{
    size_t pos = buffer.find("\r\n");
    if (pos == string::npos)
        throw std::runtime_error(ERR_NO_REQUEST_LINE);

    string requestLine = buffer.substr(0, pos);
    buffer.erase(0, pos + 2);

    size_t methodEnd = requestLine.find(' ');
    if (methodEnd == string::npos)
        throw std::runtime_error(ERR_NO_METHOD);
    setMethod(requestLine.substr(0, methodEnd));
    requestLine.erase(0, methodEnd + 1);

    size_t targetEnd = requestLine.find(' ');
    if (targetEnd == string::npos)
        throw std::runtime_error(ERR_NO_TARGET);
    setTarget(requestLine.substr(0, targetEnd));
    requestLine.erase(0, targetEnd + 1);
    setProtocolVersion(requestLine);

    while (!buffer.empty())
    {
        pos = buffer.find("\r\n");
        if (pos == string::npos)
            throw std::runtime_error(ERR_INVALID_HEADER_FMT);

        string headerLine = buffer.substr(0, pos);
        buffer.erase(0, pos + 2);

        if (headerLine.empty())
            break;

        size_t colonPos = headerLine.find(':');
        if (colonPos == string::npos)
            throw std::runtime_error(ERR_NO_COLON);

        string key = headerLine.substr(0, colonPos);
        string value = headerLine.substr(colonPos + 1);
        if (key.empty() && value.empty())
            throw std::runtime_error(ERR_EMPTY_KEY_VAL);
        setHeaders(key, value);
    }
}

//* parse body content based on content length
void HttpRequest::parseBodyContent(string& buffer){
    if (buffer.size() < contentLength)
        return;
    this->bodyContent = buffer.substr(0, contentLength);
    buffer.erase(0);
    is_complete = true;
}

void HttpRequest::parseChunkedBody(string& buffer)
{
    while (!buffer.empty())
    {
        if (chunk_state == READ_SIZE)
        {
            size_t posEndSize = buffer.find("\r\n");
            if (posEndSize == string::npos)
                return;

            errno = 0;
            char *end = NULL;
            long parsed_len = strtol(buffer.substr(0, posEndSize).c_str(), &end, 16);
            if (errno == ERANGE || *end != '\0' || parsed_len < 0)
                throw std::runtime_error(ERR_INVALID_HEX_SIZE);

            expectedChunkSize = static_cast<size_t>(parsed_len);

            if (expectedChunkSize == 0)
            {
                buffer.erase(0);
                is_complete = true;
                return;
            }

            buffer.erase(0, posEndSize + 2);
            chunk_state = READ_DATA;
        }

        if (chunk_state == READ_DATA)
        {
            if (buffer.size() < expectedChunkSize + 2)
                return;

            string content = buffer.substr(0, expectedChunkSize);
            if (buffer.compare(expectedChunkSize, 2, "\r\n") != 0)
                throw std::runtime_error(ERR_INVALID_CHUNK_TERM);

            buffer.erase(0, expectedChunkSize + 2);
            bodyContent.append(content);
            chunk_state = READ_SIZE;
        }
    }
}

//* parse request
void HttpRequest::parseRequest(int clientFd){
    // TODO: Step 1: Read incrementally from clientFd
    string str = readRequest(clientFd);
    // if (str.empty())
    //     return ;
    raw_buffer.append(str);

    // TODO: Step 2: Parse headers if not already done
    if (!headers_parsed)
    {
        size_t end_headers = raw_buffer.find("\r\n\r\n");
        if (end_headers == string::npos){
            return;
        }
        string headerBuffer = raw_buffer.substr(0, end_headers + 2);
        parseHeaders(headerBuffer);
        raw_buffer.erase(0, end_headers + 4);
        headers_parsed = true;
        setBodyType();
    }
    // TODO: Step 3: Parse body if headers are parsed but request is not complete
    if (headers_parsed && !is_complete)
    {
        if (body_type == CHUNKED){
            parseChunkedBody(raw_buffer);
        }
        else if (body_type == CONTENT_LENGTH){
            parseBodyContent(raw_buffer);
        }
    }
    // else{
    //     if (!debuging)
    //         debug();
    //     debuging = true;
    // }
}



void HttpRequest::debug()
{
    cout << "Method: " << this->method << '\n';
    cout << "Target: " << this->path << '\n';
    cout << "Protocol: " << this->protocolVersion << '\n';
    cout << "------------Headers-----------" << '\n';
    for (map<string, string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
        cout << it->first << ": " << it->second << '\n';
    cout << "------------Body-----------" << '\n';
    cout << this->bodyContent << '\n';
    cout << "------------------------------------" << '\n';
}
