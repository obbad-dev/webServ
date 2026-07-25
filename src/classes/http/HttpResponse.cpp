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
		return false;

    std::ostringstream tmp;
    tmp << file.rdbuf();

    if (file.bad())
        return false;

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

void HttpResponse::buildStaticResponse(const HttpRequest& request, const Server& server)
{
	const vector<LocationConf>& locations = server.getLocations();
	const LocationConf* matched_loc = NULL;
	matched_loc = getMatchingLocation(locations, request.getPath());

	string root = server.getRoot();
	if (matched_loc) {
		root = matched_loc->getRoot();
	}

	string fullPath;
	if (!realPath(root, request.getPath(), fullPath)) {
		throw HttpException(STATUS_NOT_FOUND);
	}
	// /resources/portfolio/index.html/index.html
	// cout << "fullPath: " << fullPath << endl;
	struct stat path_stat;
	if (stat(fullPath.c_str(), &path_stat) != 0) {
		throw HttpException(STATUS_NOT_FOUND);
	}

	if (S_ISDIR(path_stat.st_mode))
	{
		bool found_index = false;
		vector<string> indices;
		if (matched_loc && matched_loc->indexIsSet())
			indices = matched_loc->getIndex();
		else
			indices = server.getIndex();

		for (size_t i = 0; i < indices.size(); ++i)
		{
			string index_path = fullPath;
			if (index_path[index_path.length() - 1] != '/')
				index_path += "/";
			index_path += indices[i];

			if (stat(index_path.c_str(), &path_stat) == 0 && S_ISREG(path_stat.st_mode))
			{
				fullPath = index_path;
				found_index = true;
				break;
			}
		}

		if (!found_index)
		{
			if (matched_loc && matched_loc->hasAutoindex())
				throw HttpException(STATUS_FORBIDDEN);
			else
				throw HttpException(STATUS_FORBIDDEN);
		}
	}

	string content;
	if (!read_content(content, fullPath))
	{
		throw HttpException(STATUS_FORBIDDEN); // Could be permission issue
	}

	status_code = 200;
	message = "OK";
	response_headers["Content-Type"] = getMimeType(fullPath, "text/plain");
	response_headers["Content-Length"] = intToString(content.size());
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

				// Trim leading whitespace
				size_t start = value.find_first_not_of(" \t");
				if (start != std::string::npos)
					value = value.substr(start);

				if (key == "Status")
				{
					std::istringstream status_iss(value);
					status_iss >> status_code;
					std::getline(status_iss, message);
					size_t msg_start = message.find_first_not_of(" \t");
					if (msg_start != std::string::npos)
						message = message.substr(msg_start);
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

void makeEnvVars(FdManager &fdManager)
{
	vector<string> &env_vars = fdManager.env_vars;
	env_vars.clear();

	const map<string, string> &headers = fdManager.request.getHeaders();
	const string content_length = (fdManager.request.getBodyContent().empty()) ? "" : intToString(fdManager.request.getBodyContent().size());
	

	env_vars.push_back("REQUEST_METHOD=" + fdManager.request.getMethod());
	env_vars.push_back("QUERY_STRING=" + fdManager.request.getQuery());
	if (!content_length.empty())
		env_vars.push_back("CONTENT_LENGTH=" + content_length);
	if (!content_length.empty())
		env_vars.push_back("CONTENT_TYPE=" + (headers.find("Content-Type") != headers.end() ? headers.at("Content-Type") : ""));
	env_vars.push_back("SCRIPT_NAME=" + fdManager.request.getPath());
	env_vars.push_back("SERVER_NAME=" + fdManager.listen.ip);
	env_vars.push_back("SERVER_PORT=" + intToString(fdManager.listen.port));
	env_vars.push_back("SERVER_PROTOCOL=" + fdManager.request.getProtocolVersion());
	env_vars.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env_vars.push_back("SERVER_SOFTWARE=webServ/1.0");
}

void HttpResponse::prepareCGI(FdManager &fdManager, const string &cgiPath)
{
	int to_cgi_fd[2];
	int from_cgi_fd[2];
	if (pipe(to_cgi_fd) == -1)
		throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
	if (pipe(from_cgi_fd) == -1)
	{
		close(to_cgi_fd[READ]);
		close(to_cgi_fd[WRITE]);
		throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
	}
	fcntl(to_cgi_fd[WRITE], F_SETFL, O_NONBLOCK);
	fcntl(from_cgi_fd[READ], F_SETFL, O_NONBLOCK);

	makeEnvVars(fdManager);

	pid_t pid = fork();
	if (pid == -1)
	{
		close(to_cgi_fd[READ]);
		close(to_cgi_fd[WRITE]);
		close(from_cgi_fd[READ]);
		close(from_cgi_fd[WRITE]);
		throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
	}
	else if (pid == 0)
	{
		dup2(to_cgi_fd[READ], STDIN_FILENO);
		dup2(from_cgi_fd[WRITE], STDOUT_FILENO);

		close(to_cgi_fd[READ]);
		close(to_cgi_fd[WRITE]);
		close(from_cgi_fd[WRITE]);
		close(from_cgi_fd[READ]);
		char *env[fdManager.env_vars.size() + 1];
		for (size_t i = 0; i < fdManager.env_vars.size(); ++i)
		{
			env[i] = const_cast<char *>(fdManager.env_vars[i].c_str());
		}
		env[fdManager.env_vars.size()] = NULL;
		char cmd[] = "/usr/bin";
		char *args[] = {cmd, const_cast<char *>(cgiPath.c_str()), NULL};
		execve(args[0], args, env);
		exit(127);
	}
	else
	{
		close(to_cgi_fd[READ]);
		close(from_cgi_fd[WRITE]);
		fdManager.to_cgi_fd = to_cgi_fd[WRITE];
		fdManager.from_cgi_fd = from_cgi_fd[READ];

		fdManager.cgi_pid = pid;
		
		ServerSide::add_fd_to_epoll(fdManager.epollFd, fdManager.from_cgi_fd, EPOLLIN);
		if (fdManager.request.getBodyContent().empty()){
			close(fdManager.to_cgi_fd);
			fdManager.to_cgi_fd = -1;
		}
		else
			ServerSide::add_fd_to_epoll(fdManager.epollFd, fdManager.to_cgi_fd, EPOLLOUT);
	}
}


void HttpResponse::excuteCGI(FdManager &fdManager, int triggered_fd, uint32_t events)
{
	if (triggered_fd == fdManager.to_cgi_fd && (events & EPOLLOUT))
	{
		const std::string& body = fdManager.request.getBodyContent();
		
		ssize_t n = write(fdManager.to_cgi_fd, 
						  body.data() + fdManager.cgi_bytes_written, 
						  body.size() - fdManager.cgi_bytes_written);
		
		if (n == -1)
		{
			if (errno != EAGAIN && errno != EWOULDBLOCK)
			{
				// cout << "first remove epoll\n";
				ServerSide::remove_from_epoll(fdManager.epollFd, fdManager.to_cgi_fd);
				close(fdManager.to_cgi_fd);
				fdManager.stat_fd_to_cgi = FINISHED;
				throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
			}
		}
		else
		{
			fdManager.cgi_bytes_written += n;

			if (fdManager.cgi_bytes_written == body.size())
			{
				// cout << "second remove epoll\n";
				ServerSide::remove_from_epoll(fdManager.epollFd, fdManager.to_cgi_fd);
				close(fdManager.to_cgi_fd);
				fdManager.stat_fd_to_cgi = FINISHED;
			}
		}
    }
	
	if (triggered_fd == fdManager.from_cgi_fd && (events & EPOLLIN))
	{
		char buffer[4096];
		ssize_t n = read(fdManager.from_cgi_fd, buffer, sizeof(buffer));
		if (n == -1)
		{
			if (errno != EAGAIN && errno != EWOULDBLOCK)
			{
				// cout << "third remove epoll\n";
				ServerSide::remove_from_epoll(fdManager.epollFd, fdManager.from_cgi_fd);
				close(fdManager.from_cgi_fd);
				fdManager.stat_fd_from_cgi = FINISHED;
				fdManager.cgi_state = FINISHED;
				throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
			}
		}
		else if (n == 0)
		{
			// cout << "fourth remove epoll\n";
			ServerSide::remove_from_epoll(fdManager.epollFd, fdManager.from_cgi_fd);
			close(fdManager.from_cgi_fd);
			fdManager.stat_fd_from_cgi = FINISHED;
			fdManager.cgi_state = FINISHED;
			int status;
			waitpid(fdManager.cgi_pid, &status, 0);
			if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
				throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
		}
		else
		{
			HttpResponse &response = fdManager.response;
			response.setResponseBody(response.getResponseBody() + std::string(buffer, n));
		}
	}
}