#pragma once
#include <string>
#include <map>
#include <dirent.h>
#include "Server.hpp"
#include "HttpException.hpp"
#include "HttpRequest.hpp"

struct FdManager;

#define READ 0
#define WRITE 1

class HttpResponse
{
private:
	int status_code;
	std::string message;
	std::map<std::string, std::string> response_headers;
	std::string response_body;
	std::string response_serialized;
	size_t bytesSent;

public:
	HttpResponse();
	~HttpResponse();

	static std::string getDefaultErrorPage(int status_code, std::string message);
	void buildErrorResponse(const HttpException &e, const Server &server);

	void buildStaticResponse(FdManager &manager);
	void serializeResponse(const std::string &httpVersion);

	void setStatusCode(int status_code);
	void setMessage(const std::string &message);
	void setResponseHeader(const std::string &key, const std::string &value);
	void setResponseBody(const std::string &body);
	void setResponseBody(const char *buffer, size_t size);

	static std::string getMessageStatusOfReturn(int status_code);
	int getStatusCode() const;
	const std::string &getMessage() const;
	const std::map<std::string, std::string> &getResponseHeaders() const;
	const std::string &getResponseBody() const;

	static void prepareCGI(FdManager &fdManager, const std::string &scriptPath, const std::string &interpreterPath);
	static void excuteCGI(FdManager &fdManager, int triggered_fd, uint32_t events);
	void parseCgiOutput();
	int send_response(int fd);
	void resetObjectResponse();
};
