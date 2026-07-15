#pragma once
#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sstream>

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

    void create_response(FdManager &manager);
    int send_response(int fd);
    void init_bytes_var();
};
