#pragma once
using namespace std;
#include <string>
#include <map>

class HttpRequest
{
private:
//* buffer Reading
    string raw_buffer;

//* Request Line
    string method;
    string target;
    string protocolVersion;
//* headers 
    bool headers_parsed;
    map<string, string> headers;
//* Body Type
    enum BodyType { NONE, CONTENT_LENGTH, CHUNKED } body_type;
//* Body
    string bodyContent;
    size_t contentLength;
    size_t expectedChunkSize;
//* Request Completion
    bool is_complete;

//* Private Methods
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
    map<string, string> extensions;

    HttpRequest();
    ~HttpRequest();

    bool isComplete() const { return is_complete; }


    const map<string, string> &getHeaders() const;
    const string &getMethod() const;
    const string &getPath() const;
    const string& getProtocolVersion() const;

    void setHeaders(string key, string value);
    void setMethod(string method);
    void setTarget(string target);
    void setProtocolVersion(string version);

    void parseRequest(int clientFd);
};

