#include "HttpResponse.hpp"
#include "ServerSide.hpp"
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
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";   
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large"; 
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";      
        case 502: return "Bad Gateway";    
        case 505: return "HTTP Version Not Supported";
        case 510: return "Not Extended";
        default:  return "Unknown Status";            
    }
}

  
static std::string intToString(int number)
{
    std::stringstream ss;
    ss << number;
    return ss.str();
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

HttpResponse HttpResponse::buildErrorResponse(int& statusCode, const Server &server)
{
    HttpResponse response;
    string content;
    bool founErroPage = false;

    response.setStatusCode(statusCode);
    response.setMessage(getDefaultStatusMessage(statusCode));
    map <int, string> ErrPages = server.getErrorsPages(); 

    if (ErrPages.find(statusCode) != ErrPages.end())
    {
        string fullPath = server.getRoot() + ErrPages[statusCode];
        if (read_content(content, fullPath)){
            founErroPage = true;
            response.setResponseHeader("Content-Type", getMimeTypeErrPage(fullPath));
            response.setResponseHeader("Content-Lenght", intToString(content.size()));
            response.setResponseBody(content);
        }
    }
    if (!founErroPage)
    {
        content = getDefaultErrorPage(statusCode, getDefaultStatusMessage(statusCode));
        response.setResponseHeader("Content-Type", "text/html");
        response.setResponseHeader("Content-Lenght", intToString(content.size()));
        response.setResponseBody(content);
    }
    return response;
}

//? Setters
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

string getMimeTypeErrPage(string& path)
{
    string substring;
    size_t pos = path.rfind(".");
    if (pos != string::npos)
        substring = path.substr(pos);
    else
        return "text/html";

    map<string, string>::iterator it = FdManager::extensions.find(substring);
    if (it != FdManager::extensions.end())
        return it->second;
    else
        return "text/html";
}

string retrieve_extension(map<string, string> &extensions, string &path)
{
    string substring;
    size_t pos = path.rfind(".");
    if (pos != string::npos)
        substring = path.substr(pos);
    else
        return "Not Extended";

    map<string, string>::iterator it = extensions.find(substring);
    if (it != extensions.end())
        return it->second;
    else
        return "Not Extended";
}

void HttpResponse::create_response(FdManager &manager)
{
    string response;
    if (manager.request.getMethod() == "GET")
    {
        string path;
        if (manager.request.getPath() == "/")
            path = "./resources/sites/index.html";
        else
            path = "./resources/sites" + manager.request.getPath();

        string content;
        if (read_content(content, path))
        {
            manager.response.response_body += "HTTP/1.1 200 OK\r\n";
            manager.response.response_body += "Content-Type: ";
            manager.response.response_body += retrieve_extension(FdManager::extensions, path);
            manager.response.response_body += "\r\n";
            manager.response.response_body += "Content-Length: ";
            manager.response.response_body += intToString(content.size());
            manager.response.response_body += "\r\n";
            manager.response.response_body += "\r\n";
            manager.response.response_body += content;
        }
        else
        {
            manager.response.response_body += "HTTP/1.1 404 Not Found\r\n";
            manager.response.response_body += "Content-Type: text/plain\r\n";
            manager.response.response_body += "Content-Length: 9\r\n";
            manager.response.response_body += "\r\n";
            manager.response.response_body += "Not Found";
        }
    }
}

void HttpResponse::send_response()
{
    
}
