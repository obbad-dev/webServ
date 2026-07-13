#pragma once
#include <string>
#include <map>
#include "Server.hpp"
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
        static string getDefaultStatusMessage(int status_code);
        static string getDefaultErrorPage(int status_code, string message);
        static HttpResponse buildErrorResponse(int& status_code, Server *server );

};


