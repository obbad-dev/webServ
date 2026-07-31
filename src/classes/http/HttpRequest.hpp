#pragma once
#include <string>
#include <map>
#include "HttpErrors.hpp"
struct FdManager;

class Server;

class HttpRequest
{
private:
	std::string raw_buffer;

	std::string method;
	std::string path;
	std::string protocolVersion;
	std::string queryString;

	bool headers_parsed;
	std::map<std::string, std::string> headers;

	bool _keep_alive;

	enum BodyType
	{
		NONE,
		CONTENT_LENGTH,
		CHUNKED
	} body_type;

	std::string bodyContent;
	size_t contentLength;
	size_t expectedChunkSize;

	bool is_complete;

	void determineConnectionStatus();
	void setBodyType();
	bool readRequest(int &clientFd);
	void parseBodyContent(std::string &buffer);
	void parseChunkedBody(std::string &buffer);
	void parseHeaders(std::string &buffer);
	void determineClientMaxBodySize(FdManager &fdManager);

	bool debuging;

	enum ChunkState
	{
		READ_SIZE,
		READ_DATA
	};
	ChunkState chunk_state;

public:
	HttpRequest();
	~HttpRequest();

	bool isComplete() const { return is_complete; }

	const std::map<std::string, std::string> &getHeaders() const;
	const std::string &getMethod() const;
	const std::string &getTarget() const;
	const std::string &getProtocolVersion() const;
	const std::string &getBodyContent() const { return bodyContent; }
	const std::string &getQuery() const { return queryString; }

	bool isKeepAlive() const;
	void setHeaders(std::string key, std::string value);
	void setMethod(std::string method);
	void setTarget(std::string target);
	void setProtocolVersion(std::string version);

	bool parseRequest(int clientFd, FdManager &fdManager);
	bool isCgi(std::string &script_path, std::string &interpreter_path, FdManager &manager);
	void resetRequest();
};
