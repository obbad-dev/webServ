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
    map<string, string> headers;
    string readRequest(int &clientFd);

public:
    map<string, string> extensions;

    HttpRequest();
    ~HttpRequest();
    
    const map<string, string> &getHeaders() const;
    const string &getMethod() const;
    const string &getPath() const;
    const string& getProtocolVersion() const;

    void setHeaders(string key, string value);
    void setMethod(string method);
    void setTarget(string target);
    void setProtocolVersion(string version);

    void parseRequest(int &clientFd);
    void create_response(int &clientFd);
};
