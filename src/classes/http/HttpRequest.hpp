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

public:
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
};
