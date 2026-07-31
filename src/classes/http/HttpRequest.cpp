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
#include "ServerSide.hpp"
// struct FdManager;

HttpRequest::HttpRequest() : method(""), path("")
{
	headers_parsed = false;
	contentLength = 0;
	expectedChunkSize = 0;
	chunk_state = READ_SIZE;
	is_complete = false;
	debuging = false;
	_keep_alive = false;
}
HttpRequest::~HttpRequest() {}

const map<string, string> &HttpRequest::getHeaders() const
{
	return this->headers;
}
const string &HttpRequest::getMethod() const
{
	return this->method;
}
const string &HttpRequest::getTarget() const
{
	return this->path;
}
const string &HttpRequest::getProtocolVersion() const
{
	return this->protocolVersion;
}
bool HttpRequest::isKeepAlive() const { return _keep_alive; }

void HttpRequest::setHeaders(string key, string value)
{
	while (value[0] == ' ' || value[0] == '\t')
	{
		value.erase(0, 1);
	}
	transform(key.begin(), key.end(), key.begin(), ::tolower);
	headers[key] = value;
}

void HttpRequest::setMethod(string method)
{
	if (method.empty())
		throw HttpException(STATUS_METHOD_NOT_ALLOWED);
	this->method = method;
}

static string urlDecode(const string &src)
{
	string result;

	result.reserve(src.size());
	for (size_t i = 0; i < src.size(); ++i)
	{
		if (src[i] == '%' && i + 2 < src.size())
		{
			char hex[3] = {src[i + 1], src[i + 2], '\0'};
			char *end;
			long val = strtol(hex, &end, 16);
			if (*end == '\0' && val > 0 && val < 127)
			{
				result.append(1, static_cast<char>(val));
				i += 2;
				continue;
			}
		}
		result += src[i];
	}
	return result;
}

void HttpRequest::setTarget(string target)
{
	if (target.empty() || target[0] != '/')
		throw HttpException(ERR_INVALID_TARGET);

	size_t pos = target.find("?");
	if (pos != string::npos)
	{
		this->path = urlDecode(target.substr(0, pos));
		this->queryString = target.substr(pos + 1);
	}
	else
	{
		this->path = urlDecode(target);
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
	if (headers.find("transfer-encoding") != headers.end() && headers["transfer-encoding"] == "chunked")
	{
		body_type = CHUNKED;
	}
	else if (headers.find("content-length") != headers.end())
	{
		body_type = CONTENT_LENGTH;
		char *endPtr;
		errno = 0;
		long number = strtol(headers["content-length"].c_str(), &endPtr, 10);
		if (*endPtr != '\0' || errno == ERANGE || number < 0)
			throw HttpException(ERR_INVALID_CONTENT_LEN);
		contentLength = static_cast<size_t>(number);
	}
	else
	{
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



bool HttpRequest::readRequest(int &clientFd)
{
	char *buffer = new char[65536];
	bzero(buffer, 65536);

	ssize_t byteRead = recv(clientFd, buffer, 65536, 0);
	if (byteRead == 0){
		delete[] buffer;
		return false;
	}

	if (byteRead < 0)
	{
		delete[] buffer;
		throw HttpException(ERR_READ);
	}
	else
		raw_buffer.append(buffer, byteRead);
	delete[] buffer;
	return true;
}

void HttpRequest::parseHeaders(string &buffer)
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

void HttpRequest::parseBodyContent(string &buffer)
{
	if (buffer.size() < contentLength)
		return;
	this->bodyContent = buffer.substr(0, contentLength);
	buffer.erase(0, contentLength);
	is_complete = true;
}

void HttpRequest::parseChunkedBody(string &buffer)
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
				throw HttpException(ERR_INVALID_CHUNK_TERM);
			}
			buffer.erase(0, expectedChunkSize + 2);
			bodyContent.append(content);
			chunk_state = READ_SIZE;
		}
	}
}

void HttpRequest::determineClientMaxBodySize(FdManager &fdManager)
{
	fdManager.target_path = fdManager.request.getTarget();
	fdManager.location = const_cast<LocationConf *>(getMatchingLocation(fdManager.blockServer.getLocations(), fdManager.target_path));

	if (fdManager.location && fdManager.location->hasClientMaxBodySize())
	{
		fdManager.client_max_body_size = static_cast<size_t>(fdManager.location->getClientMaxBodySize());
	}
}

bool HttpRequest::parseRequest(int clientFd, FdManager &fdManager)
{

	if (!readRequest(clientFd))
		return false;

	if (!headers_parsed)
	{
		size_t end_headers = raw_buffer.find("\r\n\r\n");
		if (end_headers == string::npos)
		{
			return true;
		}
		string headerBuffer = raw_buffer.substr(0, end_headers + 2);
		parseHeaders(headerBuffer);
		raw_buffer.erase(0, end_headers + 4);
		setBodyType();
		determineConnectionStatus();
		determineClientMaxBodySize(fdManager);
		headers_parsed = true;
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

bool HttpRequest::isCgi(string &script_path, string &interpreter_path, FdManager &manager)
{
	size_t dot_pos = path.find_last_of('.');

	if (dot_pos == string::npos)
		return false;
	string ext = path.substr(dot_pos);
	string &path_copy = manager.target_path;

	if (manager.location)
	{
		const pair<string, string> &cgiPass = manager.location->getCgiPass();
		if (manager.location->hasCgiPass() && ext == cgiPass.first)
		{
			string root = manager.location->getRoot();
			realPath(root, path_copy, script_path);
			interpreter_path = cgiPass.second;
			return true;
		}
	}

	return false;
}

void HttpRequest::resetRequest()
{
	raw_buffer.clear();
	method.clear();
	path.clear();
	protocolVersion.clear();
	queryString.clear();
	headers.clear();
	_keep_alive = false;
	body_type = NONE;
	bodyContent.clear();
	contentLength = 0;
	expectedChunkSize = 0;
	is_complete = false;
	headers_parsed = false;
	chunk_state = READ_SIZE;
}