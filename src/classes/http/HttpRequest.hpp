#pragma once
using namespace std;
#include <string>
#include <map>

class HttpRequest
{
private:
    string method;
    string path;
    map<string, string> headers;

public:
    HttpRequest();
    ~HttpRequest();
    
    const map<string, string> &getHeaders() const;
    const string &getMethod() const;
    const string &getPath() const;

    void setHeaders(string key, string value);
    void setMethod(string method);
    void setPath(string path);
};
