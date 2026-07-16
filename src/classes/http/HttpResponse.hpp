#pragma once
#include <string>
#include <map>
#include "Server.hpp"

using namespace std;

struct FdManager;

class HttpResponse
{
private:
    int status_code;
    string message;
    map<string, string> response_headers;
    string response_body;
    size_t bytesSent;

    public:
        HttpResponse();
        ~HttpResponse();

        static string getDefaultStatusMessage(int status_code);
        static string getDefaultErrorPage(int status_code, string message);
        static HttpResponse buildErrorResponse(int status_code, const Server &server);
        
        void setStatusCode(int status_code);
        void setMessage(const string &message);
        void setResponseHeader(const string &key, const string &value);
        void setResponseBody(const string &body);
        int getStatusCode() const;
        const string& getMessage() const;
        const map<string, string>& getResponseHeaders() const;
        const string& getResponseBody() const;

        void create_response(FdManager &manager);
        int send_response(int fd);
        void init_bytes_var();
};

