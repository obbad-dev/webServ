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
		// cout << "problem open\n";
		return false;
	}

    std::ostringstream tmp;
    tmp << file.rdbuf();

    if (file.bad())
	{
		// cout << "problem bad\n";
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

// void HttpResponse::buildStaticResponse(const HttpRequest& request, const Server& server)
// {
	// const vector<LocationConf>& locations = server.getLocations();
	// const LocationConf* matched_loc = NULL;
	// matched_loc = getMatchingLocation(locations, request.getPath());

	// string root = server.getRoot();
	// if (matched_loc) {
	// 	root = matched_loc->getRoot();
	// }

	// string fullPath;
	// if (!realPath(root, request.getPath(), fullPath)) {
	// 	throw HttpException(STATUS_NOT_FOUND);
	// }
	// // /resources/portfolio/index.html/index.html
	// // cout << "fullPath: " << fullPath << endl;
	// struct stat path_stat;
	// if (stat(fullPath.c_str(), &path_stat) != 0) {
	// 	throw HttpException(STATUS_NOT_FOUND);
	// }

	// if (S_ISDIR(path_stat.st_mode))
	// {
	// 	bool found_index = false;
	// 	vector<string> indices;
	// 	if (matched_loc && matched_loc->indexIsSet())
	// 		indices = matched_loc->getIndex();
	// 	else
	// 		indices = server.getIndex();

	// 	for (size_t i = 0; i < indices.size(); ++i)
	// 	{
	// 		string index_path = fullPath;
	// 		if (index_path[index_path.length() - 1] != '/')
	// 			index_path += "/";
	// 		index_path += indices[i];

	// 		if (stat(index_path.c_str(), &path_stat) == 0 && S_ISREG(path_stat.st_mode))
	// 		{
	// 			fullPath = index_path;
	// 			found_index = true;
	// 			break;
	// 		}
	// 	}

	// 	if (!found_index)
	// 	{
	// 		if (matched_loc && matched_loc->hasAutoindex())
	// 			throw HttpException(STATUS_FORBIDDEN);
	// 		else
	// 			throw HttpException(STATUS_FORBIDDEN);
	// 	}
	// }

	// string content;
	// if (!read_content(content, fullPath))
	// {
	// 	throw HttpException(STATUS_FORBIDDEN); // Could be permission issue
	// }

	// status_code = 200;
	// message = "OK";
	// response_headers["Content-Type"] = getMimeType(fullPath, "text/plain");
	// response_headers["Content-Length"] = intToString(content.size());
	// response_body = content;
// }

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

void HttpResponse::buildStaticResponse(FdManager &manager)
{
	HttpRequest &request = manager.request;
    HttpResponse &response = manager.response;
    const Server &server = manager.blockServer;

	// cout << "requested path: " << request.getPath() << '\n';

	if (server.hasSetClientMaxBodySize() && request.getBodyContent().size() > server.getClientMaxBodySize())
	{
		throw HttpException(STATUS_PAYLOAD_TOO_LARGE);
	}

	// 2. Find matching location (Longest Prefix Match)
	const LocationConf *location = getMatchingLocation(server.getLocations(), request.getPath());

	// 3. Handle Redirection (301 / 302)
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

	// 4. Method Verification (405 Method Not Allowed)
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

	// 5. Construct Physical Path
	string root = (location && location->rootIsSet()) ? location->getRoot() : server.getRoot();
	string physicalPath;
	if (!realPath(root, request.getPath(), physicalPath))
	{
		// cout << "forbidden 1\n";
		throw HttpException(STATUS_FORBIDDEN); // Escaping the root directory structure
	}

	// cout << "physical path before: " << physicalPath << "\n";
	// 6. File vs Directory Resolution
	struct stat pathStat;
	if (stat(physicalPath.c_str(), &pathStat) != 0)
	{
		throw HttpException(STATUS_NOT_FOUND);
	}

	if (S_ISDIR(pathStat.st_mode))
	{
		// cout << "is dir\n";
		const vector<string> &indexes = (location && location->indexIsSet()) ? location->getIndex() : server.getIndex();
		bool indexFound = false;
		for (size_t i = 0; i < indexes.size(); ++i)
		{
			string testIndex = physicalPath;
			if (testIndex[testIndex.size() - 1] != '/')
				testIndex += "/";
			testIndex += indexes[i];

			// cout << "Test index: " << testIndex << "\n";
			struct stat indexStat;
			if (stat(testIndex.c_str(), &indexStat) == 0 && S_ISREG(indexStat.st_mode))
			{
				// cout << "enter the if\n";
				physicalPath = testIndex;
				indexFound = true;
				break;
			}
		}

		if (!indexFound)
		{
			// cout << "index not found\n";
			if (location && location->hasAutoindex())
			{
				string listing = generateDirectoryListing(physicalPath, request.getPath());
				if (listing.empty())
				{
					// cout << "Server error 1\n";
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
				// cout << "forbidden 2\n";
				throw HttpException(STATUS_FORBIDDEN);
			}
		}
	}
	// cout << "physical path after: " << physicalPath << "\n";

	if (stat(physicalPath.c_str(), &pathStat) != 0)
	{
		throw HttpException(STATUS_NOT_FOUND);
	}

	if (!S_ISREG(pathStat.st_mode))
	{
		// cout << "forbidden 3\n";
		throw HttpException(STATUS_FORBIDDEN);
	}

	if (request.getMethod() == "GET")
	{
		string body;
		if (!read_content(body, physicalPath))
		{
			// cout << "Server error 2\n";
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
			if (uploadDir.empty())
				uploadDir = ".";

			string filename = request.getPath();
			size_t pos = filename.rfind('/');
			if (pos != string::npos)
				filename = filename.substr(pos + 1);

			if (filename.empty() || filename == "upload")
			{
				filename = "upload_" + intToString(time(NULL));
			}

			string finalUploadPath = uploadDir;
			if (finalUploadPath.empty() || finalUploadPath[finalUploadPath.size() - 1] != '/')
				finalUploadPath += "/";
			finalUploadPath += filename;

			ofstream outFile(finalUploadPath.c_str(), ios::binary);
			if (!outFile.is_open())
			{
				// cout << "Server error 3\n";
				throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
			}
			const string &fileData = request.getBodyContent();
			outFile.write(fileData.data(), fileData.size());
			outFile.close();

			response.setStatusCode(201);
			response.setMessage("Created");
			response.setResponseHeader("Content-Type", "text/plain");
			string bodyContent = "File uploaded successfully: " + filename;
			response.setResponseHeader("Content-Length", intToString(bodyContent.size()));
			response.setResponseBody(bodyContent);
		}
		else
		{
			throw HttpException(STATUS_METHOD_NOT_ALLOWED);
		}
	}
	else if (request.getMethod() == "DELETE")
	{
		if (std::remove(physicalPath.c_str()) == 0)
		{
			response.setStatusCode(204);
			response.setMessage("No Content");
			response.setResponseBody("");
		}
		else
		{
			// cout << "forbidden 4\n";
			throw HttpException(STATUS_FORBIDDEN);
		}
	}

	response.serializeResponse(request.getProtocolVersion().empty() ? "HTTP/1.1" : request.getProtocolVersion());
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

				// Trim leading and trailing whitespace for value
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
					
					// Trim leading and trailing whitespace for message
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

//? Getters
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

