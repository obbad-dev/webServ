#pragma once
#include <string>
#include <map>

using namespace std;

class HttpResponse
{
private:
    int status_code;
    string message;
    map<string, string> response_headers;
    string response_body;

public:
    HttpResponse();
    ~HttpResponse();

    void create_response(int clientFd, map<string, string> &extensions);
};

HttpResponse::HttpResponse()
{
}

HttpResponse::~HttpResponse()
{
}
