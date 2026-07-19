#include "HttpResponse.hpp"
#include "ServerSide.hpp"
#include "helperFunc.hpp"
#include <sstream>
#include <iostream>
#include <fstream>
#include <unistd.h>

HttpResponse::HttpResponse()
{
    status_code = 0;
    message = "";

}

HttpResponse::~HttpResponse()
{
}

std::string HttpResponse::getDefaultStatusMessage(int statusCode)
{
    switch (statusCode)
    {
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";            
        default:  return "Unknown Status";            
    }
}

std::string HttpResponse::getDefaultErrorPage(int statusCode, std::string message)
{
    std::string html;

    html.append("<!DOCTYPE html>\n<html>\n<head>\n<title>");
    html.append(intToString(statusCode));
    html.append(" ");
    html.append(message);
    html.append("</title>\n<style>\n");
    html.append("body { font-family: sans-serif; background-color: #f7f9fa; color: #333; text-align: center; padding: 50px; }\n");
    html.append("h1 { font-size: 50px; color: #e74c3c; }\n");
    html.append("p { font-size: 20px; color: #666; }\n");
    html.append("hr { max-width: 50px; border: 1px solid #ccc; margin: 30px auto; }\n");
    html.append("</style>\n</head>\n<body>\n<h1>");
    html.append(intToString(statusCode));
    html.append("</h1>\n<p>");
    html.append(message);
    html.append("</p>\n<hr>\n<p style=\"font-size: 14px; color: #999;\">webServ/1.0</p>\n</body>\n</html>");

    return html;
}

bool read_content(string &content, string &path)
{
    ifstream file(path.c_str());
    if (!file.is_open())
        return false;

    string tmp_content;
    while (1)
    {
        getline(file, tmp_content);
        content += tmp_content;
        if (file.eof())
            break;
        content += "\n";
    }
    return true;
}

void HttpResponse::buildErrorResponse(const HttpException &e, const Server &server)
{
    string content;
    bool founErroPage = false;

    status_code = e.getStatusCode();
    message = e.getStatusMessage();

    const map <int, string>& ErrPages = server.getErrorsPages(); 
    map <int, string>::const_iterator it = ErrPages.find(e.getStatusCode() );

    if ( it != ErrPages.end())
    {
        string fullPath;
        if (realPath(server.getRoot(), it->second, fullPath) && read_content(content, fullPath)){
            founErroPage = true;
            response_headers["Content-Type"] = getMimeType(fullPath, ERR_TYPE_FILE);
            response_headers["Content-Length"] = intToString(content.size());
        }
    }
    if (!founErroPage)
    {
        content = getDefaultErrorPage(e.getStatusCode(), e.getStatusMessage());
        response_headers["Content-Type"] = "text/html";
        response_headers["Content-Length"] = intToString(content.size());
    }
    response_body = content;
}

void HttpResponse::serializeResponse(string httpVersion)
{
    response_serialized.clear();
    response_serialized.append(httpVersion + " " + intToString(status_code) + " " + message + "\r\n");
    for (std::map<std::string, std::string>::const_iterator it = response_headers.begin(); it != response_headers.end(); ++it)
    {
        response_serialized.append(it->first + ": " + it->second + "\r\n");
    }
    response_serialized.append("\r\n");
    response_serialized.append(response_body);
}

void HttpResponse::setMessage(const std::string &message)
{
    this->message = message;
}
void HttpResponse::setResponseHeader(const std::string &key, const std::string &value)
{
    response_headers[key] = value;
}
void HttpResponse::setResponseBody(const std::string &body)
{
    response_body = body;
}

//? Getters
int HttpResponse::getStatusCode() const
{
    return status_code;
}
const string& HttpResponse::getMessage() const
{
    return message;
}
const map<string, string>& HttpResponse::getResponseHeaders() const
{
    return response_headers;
}
const string& HttpResponse::getResponseBody() const
{
    return response_body;
}
void HttpResponse::setStatusCode(int status_code)
{
    this->status_code = status_code;
}

void HttpResponse::init_bytes_var() { bytesSent = 0;}

int HttpResponse::send_response(int fd)
{
    while (bytesSent < response_serialized.size())
    {
        ssize_t n = send(fd, (response_serialized.data() + bytesSent), (response_serialized.size() - bytesSent), 0);
        if (n == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return 0;
            return -1;
        }
        bytesSent += n;
    }
    return 1;
}
