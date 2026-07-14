#include "HttpResponse.hpp"
#include "ServerSide.hpp"

HttpResponse::HttpResponse() {}

HttpResponse::~HttpResponse() {}

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

string conv_to_str(int number)
{
    ostringstream s;
    s << number;
    return s.str();
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
            manager.response.response_body += conv_to_str(content.size());
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
    // else if (method == "POST")
    // {

    // }
    // else if (method = "DELETE")
    // {

    // }
    // else
    // {
    //     // send ERROR;
    // }
}

void HttpResponse::send_response()
{
    
}
