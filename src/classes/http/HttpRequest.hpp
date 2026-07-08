#pragma once
using namespace std;
#include <string>
#include <map>

class HttpRequest
{
private:
    string method;
    string target;
    string protocolVersion;
    string bodyContent;
    string raw_buffer;
    bool headers_parsed;
    
    map<string, string> headers;
    string readRequest(int &clientFd);
    void parseBody(int &clientFd, string& request);
    void parseChunkedBody(int &clientFd, string& request);
    void debug();

    // TODO: Define incremental state-tracking variables:
    // - std::string raw_buffer; (to accumulate socket read bytes)
    // - bool headers_parsed; (initialize to false in constructor)
    // - bool is_complete; (initialize to false in constructor)
    // - enum BodyType { NONE, CONTENT_LENGTH, CHUNKED } body_type; (initialize to NONE)
    // - size_t content_length; (initialize to 0)
    // - enum ChunkState { READ_SIZE, READ_DATA } chunk_state; (initialize to READ_SIZE)
    // - size_t expected_chunk_size; (initialize to 0)

public:
    map<string, string> extensions;

    HttpRequest();
    ~HttpRequest();

    // TODO: Add getter to check completion:
    // bool isComplete() const;


    const map<string, string> &getHeaders() const;
    const string &getMethod() const;
    const string &getPath() const;
    const string& getProtocolVersion() const;

    void setHeaders(string key, string value);
    void setMethod(string method);
    void setTarget(string target);
    void setProtocolVersion(string version);
    void setBodyContent(string& body);

    void parseRequest(int clientFd);
    void create_response(int clientFd);
};

