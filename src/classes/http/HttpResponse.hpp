#pragma once
#include <string>
#include <map>
#include <dirent.h>
#include "Server.hpp"
#include "HttpException.hpp"
#include "HttpRequest.hpp"

using namespace std;

struct FdManager;

#define READ 0
#define WRITE 1

class HttpResponse
{
private:
	int status_code;
	string message;
	map<string, string> response_headers;
	string response_body;
	string response_serialized;
	size_t bytesSent;

public:
	HttpResponse();
	~HttpResponse();

	static string getDefaultStatusMessage(int status_code);
	static string getDefaultErrorPage(int status_code, string message);
	void buildErrorResponse(const HttpException &e, const Server &server);
	// void buildStaticResponse(const HttpRequest& request, const Server& server);
	void buildStaticResponse(FdManager &manager);
	void serializeResponse(const string& httpVersion);

	void setStatusCode(int status_code);
	void setMessage(const string &message);
	void setResponseHeader(const string &key, const string &value);
	void setResponseBody(const string &body);
	int getStatusCode() const;
	const string &getMessage() const;
	const map<string, string> &getResponseHeaders() const;
	const string &getResponseBody() const;

	static void prepareCGI(FdManager &fdManager, const string &cgiPath);
	static void excuteCGI(FdManager &fdManager, int triggered_fd, uint32_t events);
	void parseCgiOutput();
	int send_response(int fd);
	void resetObjectResponse();
};
