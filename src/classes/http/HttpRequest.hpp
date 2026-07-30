#pragma once
using namespace std;
#include <string>
#include <map>
#include "HttpErrors.hpp"
struct FdManager;

class Server;

class HttpRequest
{
private:
	string raw_buffer;

	string method;
	string path;
	string protocolVersion;
	string queryString;

	bool headers_parsed;
	map<string, string> headers;

	bool _keep_alive;

	enum BodyType
	{
		NONE,
		CONTENT_LENGTH,
		CHUNKED
	} body_type;

	string bodyContent;
	size_t contentLength;
	size_t expectedChunkSize;

	bool is_complete;

	void determineConnectionStatus();
	void setBodyType();
	bool readRequest(int &clientFd);
	void parseBodyContent(string &buffer);
	void parseChunkedBody(string &buffer);
	void parseHeaders(string &buffer);
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

	const map<string, string> &getHeaders() const;
	const string &getMethod() const;
	const string &getTarget() const;
	const string &getProtocolVersion() const;
	const string &getBodyContent() const { return bodyContent; }
	const string &getQuery() const { return queryString; }

	bool isKeepAlive() const;
	void setHeaders(string key, string value);
	void setMethod(string method);
	void setTarget(string target);
	void setProtocolVersion(string version);

	bool parseRequest(int clientFd, FdManager &fdManager);
	bool isCgi(string &script_path, string &interpreter_path, FdManager &manager);
	void resetRequest()
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
};
