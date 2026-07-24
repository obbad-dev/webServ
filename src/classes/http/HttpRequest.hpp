#pragma once
using namespace std;
#include <string>
#include <map>
#include "HttpErrors.hpp"

class Server;

class HttpRequest
{
private:
//* buffer Reading
    string raw_buffer;

//* Request Line
    string method;
    string path;
    string protocolVersion;
	string queryString;

//* headers 
    bool headers_parsed;
    map<string, string> headers;

    bool _keep_alive;
//* Body Type
    enum BodyType { NONE, CONTENT_LENGTH, CHUNKED } body_type;
//* Body
    string bodyContent;
    size_t contentLength;
    size_t expectedChunkSize;
//* Request Completion
    bool is_complete;

//* Private Methods
    void determineConnectionStatus();
    void setBodyType();
    string readRequest(int &clientFd);
    void parseBodyContent(string& buffer);
    void parseChunkedBody(string& buffer);
    void parseHeaders(string& buffer);
    void debug();

//* for debuging
    bool debuging;

    enum ChunkState { READ_SIZE, READ_DATA };
    ChunkState chunk_state;

public:
    HttpRequest();
    ~HttpRequest();

    bool isComplete() const { return is_complete; } // change implementation to cpp


    const map<string, string> &getHeaders() const;
    const string &getMethod() const;
    const string &getPath() const;
    const string& getProtocolVersion() const;
    const string& getBodyContent() const { return bodyContent; }
	const string& getQuery() const { return queryString; }

    bool isKeepAlive() const ;
    void setHeaders(string key, string value);
    void setMethod(string method);
    void setTarget(string target);
    void setProtocolVersion(string version);

    bool parseRequest(int clientFd);
    bool isCgi(const Server& server, string& script_path) const;
	void resetRequest() {
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

