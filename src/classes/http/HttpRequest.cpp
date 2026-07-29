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
#include "HttpException.hpp"
#include "Server.hpp"
#include "helperFunc.hpp"

HttpRequest::HttpRequest(): method(""), path("") {
    headers_parsed = false;
    contentLength = 0;
    expectedChunkSize = 0;
    chunk_state = READ_SIZE;
    is_complete = false;
    debuging = false;
    _keep_alive = false;
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
bool HttpRequest::isKeepAlive() const { return _keep_alive; }

void HttpRequest::setHeaders(string key, string value){
    while (value[0] == ' ' || value[0] == '\t')
    {
        value.erase(0, 1);
    }
    transform(key.begin(), key.end(), key.begin(), ::tolower);
    headers[key] = value;
}
void HttpRequest::setMethod(string method){
    if (method.empty())
        throw HttpException(STATUS_METHOD_NOT_ALLOWED);
    this->method = method;
}
void HttpRequest::setTarget(string target){
	if (target.empty() || target[0] != '/')
		throw HttpException(ERR_INVALID_TARGET);
	
	size_t pos = target.find("?");
	if (pos != string::npos)
	{
		this->path = target.substr(0, pos);
		this->queryString = target.substr(pos + 1);
	}
	else{
    	this->path = target;
		this->queryString = "";
	}
}
void HttpRequest::setProtocolVersion(string version)
{
    if (version.empty())
        throw HttpException(ERR_INVALID_PROTOCOL);
    if (version != "HTTP/1.1" && version != "HTTP/1.0")
        throw HttpException(ERR_UNSUPPORTED_VERSION);
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
            throw HttpException(ERR_INVALID_CONTENT_LEN);
        contentLength = number;
    }
    else{
        body_type = NONE;
        is_complete = true;
    }
}

void HttpRequest::determineConnectionStatus() 
{
    if (headers.find("connection") != headers.end())
    {
        if (headers["connection"] == "keep-alive")
            _keep_alive = true;
	}
	else if (protocolVersion == "HTTP/1.1")
	{
		_keep_alive = true;
    }
}

//read Request
bool HttpRequest::readRequest(int& clientFd)
{
    char buffer[4096];
    memset(buffer, 0, sizeof(buffer)); 

    ssize_t byteRead = recv(clientFd, buffer, sizeof(buffer), 0);
    if (byteRead == 0)
        return false;
    if (byteRead < 0)
	{
		cout << "!!!!!! Error in read: " << strerror(errno) << "\n";
        throw HttpException(ERR_READ);
	}
	else
		raw_buffer.append(buffer, byteRead);
	return true;
	
}

void HttpRequest::parseHeaders(string& buffer)
{
    size_t pos = buffer.find("\r\n");
    if (pos == string::npos)
        throw HttpException(ERR_NO_REQUEST_LINE);

    string requestLine = buffer.substr(0, pos);
    buffer.erase(0, pos + 2);

    size_t methodEnd = requestLine.find(' ');
    if (methodEnd == string::npos)
        throw HttpException(ERR_NO_METHOD);
    setMethod(requestLine.substr(0, methodEnd));
    requestLine.erase(0, methodEnd + 1);

    size_t targetEnd = requestLine.find(' ');
    if (targetEnd == string::npos)
        throw HttpException(ERR_NO_TARGET);
    setTarget(requestLine.substr(0, targetEnd));
    requestLine.erase(0, targetEnd + 1);
    setProtocolVersion(requestLine);

    while (!buffer.empty())
    {
        pos = buffer.find("\r\n");
        if (pos == string::npos)
            throw HttpException(ERR_INVALID_HEADER_FMT);

        string headerLine = buffer.substr(0, pos);
        buffer.erase(0, pos + 2);

        if (headerLine.empty())
            break;

        size_t colonPos = headerLine.find(':');
        if (colonPos == string::npos)
            throw HttpException(ERR_NO_COLON);

        string key = headerLine.substr(0, colonPos);
        string value = headerLine.substr(colonPos + 1);
        if (key.empty() && value.empty())
            throw HttpException(ERR_EMPTY_KEY_VAL);
        setHeaders(key, value);
    }
}

//* parse body content based on content length
void HttpRequest::parseBodyContent(string& buffer){

    if (buffer.size() < contentLength) return;

    this->bodyContent = buffer.substr(0, contentLength);
    buffer.erase(0, contentLength);
    is_complete = true;;
}

// > check my parseRequest is read all request or have a problem in that logic losing data or somthing

// ▸ Thought for 5s, 389 tokens
//   Prioritizing Tool Usage

// ● Search(Search for parseRequest function) (ctrl+o to expand)

// ▸ Thought for 5s, 378 tokens
//   Prioritizing Tool Usage

// ● Read(/home/oobbad/Documents/webServ/src/classes/http/HttpRequest.cpp) (ctrl+o to expand)

// ▸ Thought for 1s, 616 tokens
//   Prioritizing Tool Usage

// ● Read(/home/oobbad/Documents/webServ/src/classes/openConnection/ServerSide.cpp) (ctrl+o to expand)

// ▸ Thought for 18s, 3.7k tokens
//   Prioritizing Tool Usage
//   I checked your request parsing logic, and there are indeed a few major problems where you are      
//   losing data or dropping requests. Here is what is going wrong (without code):
  
//   1. You are deleting unhandled data (Losing Pipelined Requests)
//   In both parseBodyContent and parseChunkedBody, when you finish reading the body you do buffer.     
//   erase(0). By default, this function deletes the entire remaining string. If a client sends multiple
//   requests on the same connection very quickly (this is called HTTP Pipelining), the second request  
//   will be sitting in the buffer right after the first request's body. By calling erase(0), you       
//   completely wipe out the next request! You should only erase the exact number of bytes you processed
//   (e.g., erasing exactly contentLength bytes).
//   2. You drop connections on normal non-blocking behavior
//   In your readRequest function, if recv() returns -1, you immediately throw a fatal ERR_READ         
//   exception. Because your sockets are non-blocking, recv() will sometimes return -1 with a specific  
//   signal (EAGAIN or EWOULDBLOCK) which simply means "the socket is temporarily empty, I have nothing 
//   to give you right now." Throwing an exception when this happens will violently close perfectly     
//   healthy connections. You must check if the error is EWOULDBLOCK, and if it is, you should just     
//   return normally and wait for the next epoll event.
//   3. You load the entire body into memory before doing anything
//   Your parseRequest waits until the entire body is downloaded into a single huge string before it    
//   allows isComplete() to become true and the CGI script to start. If a user uploads a 100 Megabyte   
//   file, your server will hold 100MB in memory all at once. This isn't strictly "losing data", but it 
//   will make your server consume enormous amounts of RAM very quickly and likely crash if a few users 
//   upload files at the same time. Ideally, large bodies should be streamed to a temporary file or     
//   directly to the CGI script as the data arrives, rather than buffered entirely in RAM. 

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
			{
				// cout << "first execpt\n";
                throw HttpException(ERR_INVALID_HEX_SIZE);
			}

            expectedChunkSize = static_cast<size_t>(parsed_len);

            if (expectedChunkSize == 0)
            {
                size_t endTrailers = buffer.find("\r\n", posEndSize + 2);
                if (endTrailers == string::npos)
                    return;
                buffer.erase(0);
                is_complete = true;
				cout << "DEBUG: Finished reading chunked body. Total size: " << bodyContent.size() << " bytes.\n";
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
			{
				// cout << "second execpt\n";
                throw HttpException(ERR_INVALID_CHUNK_TERM);
			}

            buffer.erase(0, expectedChunkSize + 2);
            bodyContent.append(content);
            chunk_state = READ_SIZE;
        }
    }
}

//* parse request
bool HttpRequest::parseRequest(int clientFd){

    if (!readRequest(clientFd))
        return false;

    if (!headers_parsed)
    {
        size_t end_headers = raw_buffer.find("\r\n\r\n");
        if (end_headers == string::npos){
            return true;
        }
        string headerBuffer = raw_buffer.substr(0, end_headers + 2);
        parseHeaders(headerBuffer);
        raw_buffer.erase(0, end_headers + 4);
        headers_parsed = true;
        setBodyType();
		determineConnectionStatus();
    }
    if (headers_parsed && !is_complete)
    {
        if (body_type == CHUNKED)
		{
            parseChunkedBody(raw_buffer);
        }
        else if (body_type == CONTENT_LENGTH)
		{
            parseBodyContent(raw_buffer);
        }
    }

    return true;
}



// void HttpRequest::debug()
// {
//     cout << "Method: " << this->method << '\n';
//     cout << "Target: " << this->path << '\n';
//     cout << "Protocol: " << this->protocolVersion << '\n';
//     cout << "------------Headers-----------" << '\n';
//     for (map<string, string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
//         cout << it->first << ": " << it->second << '\n';
//     cout << "------------Body-----------" << '\n';
//     cout << this->bodyContent << '\n';
//     cout << "------------------------------------" << '\n';
// }

bool HttpRequest::isCgi(const Server& server, string& script_path, string& interpreter_path)
{
    size_t dot_pos = path.find_last_of('.');

    if (dot_pos == string::npos) 
		return false;
    string ext = path.substr(dot_pos);
	string path_copy = path;
    const LocationConf* matched_loc = getMatchingLocation(server.getLocations(), path_copy);

    if (matched_loc) {
        const pair<string, string>& cgiPass = matched_loc->getCgiPass();
        if (matched_loc->hasCgiPass() && ext == cgiPass.first) {
            string root = matched_loc->getRoot();
            realPath(root, path_copy, script_path);
			// path = path_copy;
            interpreter_path = cgiPass.second;
            return true;
        }
    }

    return false;
}
