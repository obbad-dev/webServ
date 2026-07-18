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

string getMimeType(string& path, string msg)
{
    string substring;
    size_t pos = path.rfind(".");
    if (pos != string::npos)
        substring = path.substr(pos);
    else
        return msg;

    map<string, string>::iterator it = FdManager::extensions.find(substring);
    if (it != FdManager::extensions.end())
        return it->second;
    else
        return msg;
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


//TODO :
 /**
  ### 5. Architectural: Missing Response Serialization

  • The Flaw: The  HttpResponse  class stores headers, status code, and body in separate member variables, but lacks a method to serialize itself. In HttpResponse.cpp, you build raw HTTP
  status lines and header strings manually inside the body.
  • Conceptual Issue: Once you call  buildErrorResponse , you have a populated  HttpResponse  object. However, there is no generic function to turn this object's fields into the standard
  HTTP wire format (e.g.  HTTP/1.1 404 Not Found\r\nContent-Length: ...\r\n\r\n... ).
  • What you should do: Add a member function to  HttpResponse  (such as  std::string serialize() const ) that automatically constructs the raw HTTP response string using the status code,
  message, headers, and body. This prevents you from writing duplicate raw formatting code.

  ### 6. Catching and Mapping Exceptions to Status Codes

  • The Flaw: In ServerSide.cpp, you catch a generic  const std::exception& e  during parsing.
  • Conceptual Issue: Standard  std::runtime_error  exceptions thrown during request parsing (e.g., method unsupported, body too large) only contain a string message but no associated
  HTTP status code. The server loop will not know whether to respond with  400 ,  405 ,  413 ,  505 , or  500 .
  • What you should do: Define a custom exception class (e.g.,  HttpException ) inheriting from  std::exception  that accepts and stores an HTTP status code. Throw this custom exception
  when request parsing fails. In your main server loop, catch  HttpException  specifically, retrieve its status code, pass it to  buildErrorResponse , and fall back to status  500  for
  other unexpected exceptions.

  ### 7. File Reading Efficiency and Binary Safety

  • The Flaw: HttpResponse.cpp reads the error page file line-by-line using  std::getline .
  • Impact:
      • Reading a file line-by-line is slower and causes multiple string allocations.
      • If a user configures a binary file (like a custom image or logo) as an error page,  std::getline  will corrupt the binary data by stripping or transforming line endings.
      • The parameter  string &path  is a non-const reference, meaning you cannot pass temporary strings to it.
  • What you should do: Use  const string &path  for the path parameter, open the file in binary mode ( std::ios::binary ), and read the entire file directly into the destination string
  using the file stream's buffer ( rdbuf() ) or a  std::stringstream .
  */


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

void HttpResponse::init_bytes_var() { bytesSent = 0;}

// look into locations if the requested path is available
    // if found attach it to the root path
        // search for root inside location block if found attach it if not use the general one
// check if the generated path a dir or file
// if dir, look whether you have index file or not
// if yes work on that path
// if not check is it autoindex
// if yes list the files inside the directory

bool trim_till_slash(string &path, size_t &index)
{
    if (!index)
        return true;
    size_t pos = path.rfind("/");
    if (pos != string::npos)
    {
        path = path.substr(0, ++pos);
        return true;
    }
    return false;
}

string look_for_matching_location(const vector<LocationConf> &location_vec, string path, LocationConf &locationBlock)
{
    size_t i = 0;

    while (trim_till_slash(path, i))
    {
        cout << "path = " << path << "\n";
        for (i = 0; i < location_vec.size(); i++)
        {
            cout << "location path = " << location_vec[i].getPath() << "\n";
            if (path == location_vec[i].getPath())
            {
                cout << "matched\n";
                locationBlock = location_vec[i];
                return path;
            }
        }
        if (path == "/")
            break;
    }
    return "";
}

enum FILE_TYPE
{
    REG_FILE,
    DIRECTORY,
    OTHER
};

#include <sys/stat.h>

FILE_TYPE check_file_type(string &path)
{
    struct stat path_stat;

    if (stat(path.c_str(), &path_stat) == 0)
    {
        if (S_ISDIR(path_stat.st_mode))
        {
            cout << "\n----------- is dir -----------\n";
            return DIRECTORY;
        }
        else if (S_ISREG(path_stat.st_mode))
        {
            cout << "\n----------- is file -----------\n";
            return REG_FILE;
        }
    }
    return OTHER;
}

bool look_into_indexes(string &path, const vector<string> &indexes)
{
    for (size_t i = 0; i < indexes.size(); i++)
    {
        string concatinated_path = path + indexes[i];
        if (check_file_type(concatinated_path) == REG_FILE)
        {
            path = concatinated_path;
            return true;
        }
    }
    return false;
}

void generate_directory_listing()
{

}

void HttpResponse::create_response(FdManager &manager)
{
    string requested_path = manager.request.getPath();
    cout << "requested path: " << requested_path << "\n";
    LocationConf locationBlock;
    string matching_path = look_for_matching_location(manager.blockServer.getLocations(), requested_path, locationBlock);
    if (!matching_path.empty())
    {
        size_t index = requested_path.find(matching_path);
        if (index != string::npos)
        {
            requested_path.erase(index, matching_path.length());
        }
    
        requested_path = locationBlock.getRoot() + "/" + requested_path;
    }
    else
    {
        cout << "no matching location, using the server's root instead\n";
        requested_path = manager.blockServer.getRoot() + "/" + requested_path;
    }

    cout << "\nfinal path = " << requested_path << "\n";

    FILE_TYPE fileType = check_file_type(requested_path);
    if (fileType == DIRECTORY)
    {
        if (look_into_indexes(requested_path, manager.blockServer.getIndex()) == false)
        {
            if (locationBlock.hasAutoindex())
            {
                manager.response.setStatusCode(200);
                generate_directory_listing();
            }
            else
            {
                manager.response.setStatusCode(403);
                return ; // 403 Forbidden
            }
        }
        else
        {
            if (read_content(response_body, requested_path) == false)
            {
                manager.response.setStatusCode(404);
                return ; // couldn't open the file
            }
            manager.response.setStatusCode(200);
            return ; // respond with the file
        }
    }
    else if (fileType == REG_FILE)
    {
        cout << "the final path is: " << requested_path << "\n";
    }
    else
    {
        cout << "\n----------- returned -----------\n";
        manager.response.status_code = 404;
        return ; // unsupported type of file
    }
    // string response;
    // if (manager.request.getMethod() == "GET")
    // {
    //     string path;
    //     if (manager.request.getPath() == "/")
    //         path = "./resources/sites/index.html";
    //     else
    //         path = "./resources/sites" + manager.request.getPath();

    //     string content;
    //     if (read_content(content, path))
    //     {
    //         manager.response.response_body += "HTTP/1.1 200 OK\r\n";
    //         manager.response.response_body += "Content-Type: ";
    //         manager.response.response_body += getMimeType(path, NOT_EXTENDED);
    //         manager.response.response_body += "\r\n";
    //         manager.response.response_body += "Content-Length: ";
    //         manager.response.response_body += intToString(content.size());
    //         manager.response.response_body += "\r\n";
    //         manager.response.response_body += "\r\n";
    //         manager.response.response_body += content;
    //     }
    //     else
    //     {
    //         manager.response.response_body += "HTTP/1.1 404 Not Found\r\n";
    //         manager.response.response_body += "Content-Type: text/plain\r\n";
    //         manager.response.response_body += "Content-Length: 9\r\n";
    //         manager.response.response_body += "\r\n";
    //         manager.response.response_body += "Not Found";
    //     }
    // }
}

int HttpResponse::send_response(int fd)
{
    while (bytesSent < response_body.size())
    {
        ssize_t n = send(fd, (response_body.data() + bytesSent), (response_body.size() - bytesSent), 0);
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
