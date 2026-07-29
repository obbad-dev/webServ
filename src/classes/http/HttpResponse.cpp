
#include "HttpResponse.hpp"
#include "ServerSide.hpp"
#include "helperFunc.hpp"
#include "HttpException.hpp"
#include <sstream>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>

HttpResponse::HttpResponse()
{
	status_code = 0;
	bytesSent = 0;
	message = "";
}

HttpResponse::~HttpResponse()
{
}

std::string HttpResponse::getDefaultStatusMessage(int statusCode)
{
	switch (statusCode)
	{
	case 200:
		return "OK";
	case 201:
		return "Created";
	case 202:
		return "Accepted";
	case 204:
		return "No Content";
	case 301:
		return "Moved Permanently";
	case 302:
		return "Found";
	default:
		return "Unknown Status";
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
	ifstream file(path.c_str(), std::ios::binary);

	if (!file.is_open())
	{
		
		return false;
	}

    std::ostringstream tmp;
    tmp << file.rdbuf();

    if (file.bad())
	{
		
        return false;
	}

    content = tmp.str();

	return true;
}

void HttpResponse::buildErrorResponse(const HttpException &e, const Server &server)
{
	
	string content;
	bool founErroPage = false;

	status_code = e.getStatusCode();
	message = e.getStatusMessage();

	const map<int, string> &ErrPages = server.getErrorsPages();
	map<int, string>::const_iterator it = ErrPages.find(e.getStatusCode());

	if (it != ErrPages.end())
	{
		string fullPath;
		if (realPath(server.getRoot(), it->second, fullPath) && read_content(content, fullPath))
		{
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



	
	
	

	
	
	
	

	
	
	
	
	
	
	
	
	
	

	
	
	
	
	
	
	
	

	
	
	
	
	
	

	
	
	
	
	
	
	

	
	
	
	
	
	
	
	

	
	
	
	
	

	
	
	
	
	


string generateDirectoryListing(const string &dirPath, const string &uriPath)
{
    DIR *dir = opendir(dirPath.c_str());
    if (!dir)
    {
        return "";
    }

    stringstream ss;
    ss << "<html><head><title>Index of " << uriPath << "</title></head><body>\n";
    ss << "<h1>Index of " << uriPath << "</h1><hr><ul>\n";

    if (uriPath != "/" && !uriPath.empty())
    {
        ss << "<li><a href=\"../\">../ (Parent Directory)</a></li>\n";
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        string name = entry->d_name;
        if (name == "." || name == "..")
            continue;

        string fullPath = dirPath;
        if (fullPath.empty() || fullPath[fullPath.size() - 1] != '/')
            fullPath += "/";
        fullPath += name;

        struct stat s;
        bool isDir = false;
        if (stat(fullPath.c_str(), &s) == 0)
        {
            if (S_ISDIR(s.st_mode))
            {
                isDir = true;
            }
        }

        string linkName = name;
        if (isDir)
            linkName += "/";
            
        ss << "<li><a href=\"" << linkName << "\">" << linkName << "</a></li>\n";
    }
    closedir(dir);
    ss << "</ul><hr></body></html>";
    return ss.str();
}

bool getBoundary(string &content_type, string &boundary)
{
	size_t pos = content_type.find("boundary=");
	
	if (pos == std::string::npos)
		return false;

	boundary = content_type.substr(pos + 9);
	pos = boundary.find(';');

	if (pos != std::string::npos)
		boundary = boundary.substr(0, pos);

	if ((boundary.size() >= 2) && (boundary[0] == '"' && boundary[boundary.size() - 1] == '"'))
		boundary = boundary.substr(1, (boundary.size() - 2));

	return (!boundary.empty());
}

bool getDataFromRequest(string &filename, string &filebody, const string &request_body, string &boundary)
{
	string delimiter = "--" + boundary;
	size_t content_start = request_body.find(delimiter);
	if (content_start == std::string::npos)
		return false;

	content_start += delimiter.size();
	if (request_body.compare(content_start, 2, "\r\n") != 0)
		return false;

	content_start += 2;

	size_t headers_end = request_body.find("\r\n\r\n", content_start);
	if (headers_end == std::string::npos)
		return false;

	string part_headers = request_body.substr(content_start, headers_end - content_start);

	size_t fn_pos = part_headers.find("filename=\"");
	if (fn_pos == std::string::npos)
		return false;

	fn_pos += 10;
	size_t fn_end = part_headers.find("\"", fn_pos);
	if (fn_end == std::string::npos)
		return false;

	filename = part_headers.substr(fn_pos, fn_end - fn_pos);
	if (filename.empty())
		return false;

	size_t body_start = headers_end + 4;
	size_t body_end = request_body.find(delimiter, body_start);
	if (body_end == std::string::npos)
		return false;

	if (body_end > body_start)
	{
		if (request_body.compare(body_end - 2, 2, "\r\n") == 0)
			body_end -= 2;
		else if (request_body[body_end - 1] == '\n')
			body_end -= 1;
	}

	filebody = request_body.substr(body_start, body_end - body_start);
	return true;
}

void HttpResponse::buildStaticResponse(FdManager &manager)
{
	HttpRequest &request = manager.request;
    HttpResponse &response = manager.response;
    const Server &server = manager.blockServer;

	
	string path = request.getPath();
	const LocationConf *location = getMatchingLocation(server.getLocations(), path);
	if (location && location->hasClientMaxBodySize())
	{
		if (request.getBodyContent().size() > static_cast<size_t>(location->getClientMaxBodySize()))
			throw HttpException(STATUS_PAYLOAD_TOO_LARGE);
	}
	else if (request.getBodyContent().size() > static_cast<size_t>(server.getClientMaxBodySize()))
	{
		throw HttpException(STATUS_PAYLOAD_TOO_LARGE);
	}
	
	if (location && location->hasReturn())
	{
		pair<int, string> redir = location->getReturn();
		response.setStatusCode(redir.first);
		response.setMessage(HttpResponse::getDefaultStatusMessage(redir.first));
		response.setResponseHeader("Location", redir.second);
		response.setResponseBody("Redirecting...");
		response.serializeResponse(request.getProtocolVersion().empty() ? "HTTP/1.1" : request.getProtocolVersion());
		return;
	}

	
	if (location)
	{
		
		const set<string> &allowed = location->getAllowMethods();
		if (allowed.find(request.getMethod()) == allowed.end())
		{
			
			response.buildErrorResponse(HttpException(STATUS_METHOD_NOT_ALLOWED), server);
			string allowHeader;
			for (set<string>::const_iterator it = allowed.begin(); it != allowed.end(); ++it)
			{
				if (it != allowed.begin())
					allowHeader += ", ";
				allowHeader += *it;
			}
			response.setResponseHeader("Allow", allowHeader);
			response.serializeResponse(request.getProtocolVersion().empty() ? "HTTP/1.1" : request.getProtocolVersion());
			return;
		}
	}

	
	struct stat pathStat;
	string physicalPath;
	string root = (location && location->rootIsSet()) ? location->getRoot() : server.getRoot();
	
	

	if (!realPath(root, path, physicalPath))
	{
		
		
		throw HttpException(STATUS_FORBIDDEN); 
	}
	

	if (request.getMethod() == "GET")
	{
		
		
		if (stat(physicalPath.c_str(), &pathStat) != 0)
		{
			throw HttpException(STATUS_NOT_FOUND);
		}

		if (S_ISDIR(pathStat.st_mode))
		{
			
			const vector<string> &indexes = (location && location->indexIsSet()) ? location->getIndex() : server.getIndex();
			bool indexFound = false;
			for (size_t i = 0; i < indexes.size(); ++i)
			{
				string testIndex = physicalPath;
				if (testIndex[testIndex.size() - 1] != '/')
					testIndex += "/";
				testIndex += indexes[i];

				
				struct stat indexStat;
				if (stat(testIndex.c_str(), &indexStat) == 0 && S_ISREG(indexStat.st_mode))
				{
					
					physicalPath = testIndex;
					indexFound = true;
					break;
				}
			}

			if (!indexFound)
			{
				
				if (location && location->hasAutoindex())
				{
					string listing = generateDirectoryListing(physicalPath, path);
					if (listing.empty())
					{
						
						throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
					}
					response.setStatusCode(200);
					response.setMessage("OK");
					response.setResponseHeader("Content-Type", "text/html");
					response.setResponseHeader("Content-Length", intToString(listing.size()));
					response.setResponseBody(listing);
					response.serializeResponse(request.getProtocolVersion().empty() ? "HTTP/1.1" : request.getProtocolVersion());
					return;
				}
				else
				{
					
					throw HttpException(STATUS_NOT_FOUND);
				}
			}
		}
		

		if (stat(physicalPath.c_str(), &pathStat) != 0)
		{
			throw HttpException(STATUS_NOT_FOUND);
		}

		if (!S_ISREG(pathStat.st_mode))
		{
			
			throw HttpException(STATUS_FORBIDDEN);
		}

		string body;
		if (!read_content(body, physicalPath))
		{
			
			throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
		}
		response.setStatusCode(200);
		response.setMessage("OK");
		response.setResponseHeader("Content-Type", getMimeType(physicalPath, "application/octet-stream"));
		response.setResponseHeader("Content-Length", intToString(body.size()));
		response.setResponseBody(body);
	}
	else if (request.getMethod() == "POST")
	{
		
		if (location && location->uploadEnabledStatus())
		{
			string uploadDir = location->getUploadPath();
			
			

			if (stat(uploadDir.c_str(), &pathStat) == 0 && !S_ISDIR(pathStat.st_mode))
				throw HttpException(STATUS_INTERNAL_SERVER_ERROR);

			const map<string, string> &headers = request.getHeaders();
			map<string, string>::const_iterator it = headers.find("content-type");

			if (it == headers.end())
				throw HttpException(STATUS_BAD_REQUEST);

			string content_type = it->second;
			string finalUploadPath, filebody;

			if (content_type.find("multipart/form-data") != std::string::npos)
			{
				string boundary;
				if (getBoundary(content_type, boundary) == false)
					throw HttpException(STATUS_BAD_REQUEST);

				string filename;
				if (getDataFromRequest(filename, filebody, request.getBodyContent(), boundary) == false)
					throw HttpException(STATUS_BAD_REQUEST);

				if (realPath(uploadDir, filename, finalUploadPath) == false)
					throw HttpException(STATUS_BAD_REQUEST);
			}
			else
			{
				if (realPath(uploadDir, "file.bin", finalUploadPath) == false)
					throw HttpException(STATUS_BAD_REQUEST);

				filebody = request.getBodyContent();
			}

			ofstream outFile(finalUploadPath.c_str(), ios::binary);
			if (!outFile.is_open())
			{
				
				throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
			}

			outFile.write(filebody.data(), filebody.size());

			if (outFile.bad())
				throw HttpException(STATUS_INTERNAL_SERVER_ERROR);

			response.setStatusCode(201);
			response.setMessage("Created");
			response.setResponseHeader("Content-Type", "text/plain");
			string bodyContent = "File uploaded successfully: " + finalUploadPath;
			response.setResponseHeader("Content-Length", intToString(bodyContent.size()));
			response.setResponseBody(bodyContent);
		}
		else
		{
			response.setStatusCode(200);
			response.setMessage("OK");
			response.setResponseHeader("Content-Type", "text/plain");
			string bodyContent = "POST request received";
			response.setResponseHeader("Content-Length", intToString(bodyContent.size()));
			response.setResponseBody(bodyContent);
			
		}
	}
	else if (request.getMethod() == "DELETE")
	{
		if (access(physicalPath.c_str(), F_OK) != 0)
			throw HttpException(STATUS_NOT_FOUND);
		if (std::remove(physicalPath.c_str()) == 0)
		{
			response.setStatusCode(204);
			response.setMessage("No Content");
			response.setResponseBody("");
		}
		else
		{
			
			throw HttpException(STATUS_FORBIDDEN);
		}
	}
	const string &version = request.getProtocolVersion().empty() ? "HTTP/1.1" : request.getProtocolVersion();
	response.serializeResponse(version);
}

void HttpResponse::serializeResponse(const string& httpVersion)
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

void HttpResponse::parseCgiOutput()
{
	size_t header_end = response_body.find("\r\n\r\n");
	size_t separator_len = 4;

	if (header_end == std::string::npos)
	{
		header_end = response_body.find("\n\n");
		separator_len = 2;
	}

	if (header_end != std::string::npos)
	{
		std::string headers_str = response_body.substr(0, header_end);
		std::string new_body = response_body.substr(header_end + separator_len);

		std::istringstream iss(headers_str);
		std::string line;
		while (std::getline(iss, line))
		{
			if (!line.empty() && line[line.size() - 1] == '\r')
				line.erase(line.size() - 1);

			if (line.empty())
				continue;

			size_t colon_pos = line.find(':');
			if (colon_pos != std::string::npos)
			{
				std::string key = line.substr(0, colon_pos);
				std::string value = line.substr(colon_pos + 1);

				
				size_t start = value.find_first_not_of(" \t");
				if (start != std::string::npos) {
					size_t end = value.find_last_not_of(" \t");
					value = value.substr(start, end - start + 1);
				} else {
					value = "";
				}

				if (key == "Status" || key == "status")
				{
					std::istringstream status_iss(value);
					status_iss >> status_code;
					std::getline(status_iss, message);
					
					
					size_t msg_start = message.find_first_not_of(" \t");
					if (msg_start != std::string::npos) {
						size_t msg_end = message.find_last_not_of(" \t");
						message = message.substr(msg_start, msg_end - msg_start + 1);
					} else {
						message = "";
					}
				}
				else
				{
					response_headers[key] = value;
				}
			}
		}

		response_body = new_body;
	}

	if (status_code == 0)
	{
		status_code = 200;
		message = "OK";
	}

	response_headers["Content-Length"] = intToString(response_body.size());
}


int HttpResponse::getStatusCode() const
{
	return status_code;
}
const string &HttpResponse::getMessage() const
{
	return message;
}
const map<string, string> &HttpResponse::getResponseHeaders() const
{
	return response_headers;
}
const string &HttpResponse::getResponseBody() const
{
	return response_body;
}
void HttpResponse::setStatusCode(int status_code)
{
	this->status_code = status_code;
}

void HttpResponse::resetObjectResponse() {
	response_serialized.clear();
	status_code = 0;
	message.clear();
	response_headers.clear();
	response_body.clear();
	bytesSent = 0; 
}

int HttpResponse::send_response(int fd)
{
	ssize_t n = send(fd, (response_serialized.data() + bytesSent), (response_serialized.size() - bytesSent), 0);

	if (n == -1)
	{
		
		return -1;
	}

	bytesSent += n;
	
	if (bytesSent >= response_serialized.size())
		return 1;

	return 0;
}

