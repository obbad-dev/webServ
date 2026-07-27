#include "HttpResponse.hpp"

#include "ServerSide.hpp"
#include "helperFunc.hpp"
#include "HttpException.hpp"

#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>

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
		env_vars.push_back("CONTENT_TYPE=" + (headers.find("content-type") != headers.end() ? headers.at("content-type") : ""));
	env_vars.push_back("SCRIPT_NAME=" + fdManager.request.getPath());
	env_vars.push_back("SERVER_NAME=" + fdManager.listen.ip);
	env_vars.push_back("SERVER_PORT=" + intToString(fdManager.listen.port));
	env_vars.push_back("SERVER_PROTOCOL=" + fdManager.request.getProtocolVersion());
	env_vars.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env_vars.push_back("SERVER_SOFTWARE=webServ/1.0");
}
static bool interpreterIsExecutable(const std::string &interpreterPath)
{
	struct stat st;

	if (stat(interpreterPath.c_str(), &st) == -1 || !S_ISREG(st.st_mode))
		return false;
	if (access(interpreterPath.c_str(), X_OK) == -1)
		return false;

	return true;
}

void HttpResponse::prepareCGI(FdManager &fdManager, const string &cgiPath)
{

	if (!interpreterIsExecutable(cgiPath))
		throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
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
	if (fcntl(to_cgi_fd[WRITE], F_SETFL, O_NONBLOCK) == -1)
		throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
	if (fcntl(from_cgi_fd[READ], F_SETFL, O_NONBLOCK) == -1)
		throw HttpException(STATUS_INTERNAL_SERVER_ERROR);

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
		char cmd[] = "/usr/bin/python3";
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
			fdManager.stat_fd_to_cgi = FINISHED;
		}
		else{
			ServerSide::add_fd_to_epoll(fdManager.epollFd, fdManager.to_cgi_fd, EPOLLOUT);
		}
	}
}

static void finishCgiWrite(FdManager &fdManager)
{
	ServerSide::remove_from_epoll(fdManager.epollFd, fdManager.to_cgi_fd);
	close(fdManager.to_cgi_fd);
	fdManager.stat_fd_to_cgi = FINISHED;
}

static void handleCGIWrite(FdManager &fdManager)
{
	const std::string& body = fdManager.request.getBodyContent();
	
	ssize_t bytes_written = write(fdManager.to_cgi_fd, 
								  body.data() + fdManager.cgi_bytes_written, 
								  body.size() - fdManager.cgi_bytes_written);

	if (bytes_written == -1)
	{
		finishCgiWrite(fdManager);
		throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
	}
	fdManager.cgi_bytes_written += bytes_written;
	if (fdManager.cgi_bytes_written >= body.size())
		finishCgiWrite(fdManager);
}

static void finishCgiRead(FdManager &fdManager)
{
	ServerSide::remove_from_epoll(fdManager.epollFd, fdManager.from_cgi_fd);
	close(fdManager.from_cgi_fd);
	fdManager.stat_fd_from_cgi = FINISHED;
	fdManager.cgi_state = FINISHED;
}

static void handleCGIRead(FdManager &fdManager)
{
	char buffer[4096];
	ssize_t bytes_read = read(fdManager.from_cgi_fd, buffer, sizeof(buffer));
	
	if (bytes_read == -1)
	{
		finishCgiRead(fdManager);
		throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
	}

	if (bytes_read == 0)
	{
		finishCgiRead(fdManager);
		int status;
		waitpid(fdManager.cgi_pid, &status, 0);
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
			throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
		}
		return;
	}

	HttpResponse &response = fdManager.response;
	response.setResponseBody(response.getResponseBody() + std::string(buffer, bytes_read));
}

void HttpResponse::excuteCGI(FdManager &fdManager, int triggered_fd, uint32_t events)
{
	//! happens when the CGI process closes its read end before we finish writing all the data.
	if (triggered_fd == fdManager.to_cgi_fd && (events & (EPOLLERR | EPOLLHUP))) 
	{
		finishCgiWrite(fdManager);
		throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
	}
	//! happens when the close read end of the server like -> "close(from_cgi_fd) and epoll still monitoring it".
	if (triggered_fd == fdManager.from_cgi_fd && (events & EPOLLERR))
	{
		finishCgiRead(fdManager);
		throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
	}
	
	if (triggered_fd == fdManager.to_cgi_fd && (events & EPOLLOUT))
		handleCGIWrite(fdManager);
	if (triggered_fd == fdManager.from_cgi_fd && (events & (EPOLLIN | EPOLLHUP)))
		handleCGIRead(fdManager);
}
