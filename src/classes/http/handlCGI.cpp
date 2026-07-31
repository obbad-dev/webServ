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

	env_vars.push_back("PATH_INFO=" + fdManager.request.getTarget());

	env_vars.push_back("PATH_TRANSLATED=" + fdManager.request.getTarget());
	env_vars.push_back("REQUEST_METHOD=" + fdManager.request.getMethod());
	env_vars.push_back("QUERY_STRING=" + fdManager.request.getQuery());
	if (!content_length.empty())
		env_vars.push_back("CONTENT_LENGTH=" + content_length);
	if (!content_length.empty())
		env_vars.push_back("CONTENT_TYPE=" + (headers.find("content-type") != headers.end() ? headers.at("content-type") : ""));
	env_vars.push_back("SCRIPT_NAME=");
	env_vars.push_back("SERVER_NAME=" + fdManager.listen.ip);
	env_vars.push_back("SERVER_PORT=" + intToString(fdManager.listen.port));
	env_vars.push_back("SERVER_PROTOCOL=" + fdManager.request.getProtocolVersion());
	env_vars.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env_vars.push_back("SERVER_SOFTWARE=webServ/1.0");

	for (map<string, string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
	{
		string key = "HTTP_" + it->first;
		for (size_t i = 0; i < key.length(); ++i)
		{
			if (key[i] >= 'a' && key[i] <= 'z')
				key[i] = key[i] - 32;
			else if (key[i] == '-')
				key[i] = '_';
		}
		env_vars.push_back(key + "=" + it->second);
	}
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

static void closePipes(int to_cgi_fd[2], int from_cgi_fd[2])
{
    if (to_cgi_fd[READ] != -1)   close(to_cgi_fd[READ]);
    if (to_cgi_fd[WRITE] != -1)  close(to_cgi_fd[WRITE]);
    if (from_cgi_fd[READ] != -1) close(from_cgi_fd[READ]);
    if (from_cgi_fd[WRITE] != -1) close(from_cgi_fd[WRITE]);
}

static void createPipes(int to_cgi_fd[2], int from_cgi_fd[2])
{
    if (pipe(to_cgi_fd) == -1)
        throw HttpException(STATUS_INTERNAL_SERVER_ERROR);

    if (pipe(from_cgi_fd) == -1)
    {
        closePipes(to_cgi_fd, from_cgi_fd);
        throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
    }
}

static void setPipesNonBlocking(int to_cgi_fd[2], int from_cgi_fd[2])
{
    if (fcntl(to_cgi_fd[WRITE], F_SETFL, O_NONBLOCK) == -1 ||
        fcntl(from_cgi_fd[READ], F_SETFL, O_NONBLOCK) == -1)
    {
        closePipes(to_cgi_fd, from_cgi_fd);
        throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
    }
}

static void runCGIChild(FdManager &fdManager, const string &scriptName, const string &interpreterPath, int to_cgi_fd[2], int from_cgi_fd[2])
{
    dup2(to_cgi_fd[READ], STDIN_FILENO);
    dup2(from_cgi_fd[WRITE], STDOUT_FILENO);
    closePipes(to_cgi_fd, from_cgi_fd);

    std::vector<char *> env(fdManager.env_vars.size() + 1);
    for (size_t i = 0; i < fdManager.env_vars.size(); ++i)
        env[i] = const_cast<char *>(fdManager.env_vars[i].c_str());
    env[fdManager.env_vars.size()] = NULL;

    char *args[] = {
        const_cast<char *>(interpreterPath.c_str()),
        const_cast<char *>(scriptName.c_str()),
        NULL
    };

    execve(args[0], args, env.data());
    exit(127);
}

static void setupParentSide(FdManager &fdManager, pid_t pid,
                             int to_cgi_fd[2], int from_cgi_fd[2])
{
    close(to_cgi_fd[READ]);
    close(from_cgi_fd[WRITE]);

    fdManager.to_cgi_fd        = to_cgi_fd[WRITE];
    fdManager.from_cgi_fd      = from_cgi_fd[READ];
    fdManager.cgi_pid          = pid;
    fdManager.cgi_state        = NOT_FINISHED;
    fdManager.stat_fd_to_cgi   = NOT_FINISHED;
    fdManager.stat_fd_from_cgi = NOT_FINISHED;

    ServerSide::add_fd_to_epoll(fdManager.epollFd, fdManager.from_cgi_fd, EPOLLIN);

    if (fdManager.request.getBodyContent().empty())
    {
        close(fdManager.to_cgi_fd);
        fdManager.stat_fd_to_cgi = FINISHED;
    }
    else
    {
        ServerSide::add_fd_to_epoll(fdManager.epollFd, fdManager.to_cgi_fd, EPOLLOUT);
    }
}

void HttpResponse::prepareCGI(FdManager &fdManager, const string &scriptName, const string &interpreterPath)
{
    if (!interpreterIsExecutable(interpreterPath))
        throw HttpException(STATUS_INTERNAL_SERVER_ERROR);

    int to_cgi_fd[2]   = { -1, -1 };
    int from_cgi_fd[2] = { -1, -1 };

    createPipes(to_cgi_fd, from_cgi_fd);
    setPipesNonBlocking(to_cgi_fd, from_cgi_fd);

    makeEnvVars(fdManager);

    pid_t pid = fork();
    if (pid == -1)
    {
        closePipes(to_cgi_fd, from_cgi_fd);
        throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
    }

    if (pid == 0)
        runCGIChild(fdManager, scriptName, interpreterPath, to_cgi_fd, from_cgi_fd);
    else
        setupParentSide(fdManager, pid, to_cgi_fd, from_cgi_fd);
}

static void finishCgiWrite(FdManager &fdManager)
{
	ServerSide::remove_from_epoll(fdManager.epollFd, fdManager.to_cgi_fd);
	close(fdManager.to_cgi_fd);
	fdManager.stat_fd_to_cgi = FINISHED;
	fdManager.cgi_bytes_written = 0;
}

static void handleCGIWrite(FdManager &fdManager)
{
	const std::string &body = fdManager.request.getBodyContent();

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
	char *buffer = new char[65536];
	bzero(buffer, 65536);
	ssize_t bytes_read = read(fdManager.from_cgi_fd, buffer, 65536);

	if (bytes_read == -1)
	{
		delete[] buffer;
		finishCgiRead(fdManager);
		throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
	}

	if (bytes_read == 0)
	{
		delete[] buffer;
		finishCgiRead(fdManager);
		int status;
		waitpid(fdManager.cgi_pid, &status, 0);
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		{
			throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
		}
		return;
	}
	HttpResponse &response = fdManager.response;
	response.setResponseBody(buffer, bytes_read);
	delete[] buffer;
}

void HttpResponse::excuteCGI(FdManager &fdManager, int triggered_fd, uint32_t events)
{

	if (triggered_fd == fdManager.to_cgi_fd && (events & (EPOLLERR | EPOLLHUP)))
	{
		finishCgiWrite(fdManager);
		throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
	}

	if (triggered_fd == fdManager.from_cgi_fd && (events & EPOLLERR))
	{
		finishCgiRead(fdManager);
		throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
	}

	if (triggered_fd == fdManager.to_cgi_fd && (events & EPOLLOUT))
	{
		handleCGIWrite(fdManager);
	}
	if (triggered_fd == fdManager.from_cgi_fd && (events & (EPOLLIN | EPOLLHUP)))
	{
		handleCGIRead(fdManager);
	}
}
