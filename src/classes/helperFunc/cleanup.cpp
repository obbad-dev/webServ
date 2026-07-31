#include "helperFunc.hpp"
#include "ServerSide.hpp"

#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

void cleanupCgi(FdManager &manager, std::map<int, int> &cgiToClient)
{

	if (manager.stat_fd_to_cgi == NOT_FINISHED)
	{
		ServerSide::remove_from_epoll(manager.epollFd, manager.to_cgi_fd);
		close(manager.to_cgi_fd);
		cgiToClient.erase(manager.to_cgi_fd);
		manager.stat_fd_to_cgi = FINISHED;
	}

	if (manager.stat_fd_from_cgi == NOT_FINISHED)
	{
		ServerSide::remove_from_epoll(manager.epollFd, manager.from_cgi_fd);
		close(manager.from_cgi_fd);
		cgiToClient.erase(manager.from_cgi_fd);
		manager.stat_fd_from_cgi = FINISHED;
	}
	if (manager.cgi_pid > 0)
	{
		kill(manager.cgi_pid, SIGKILL);
		waitpid(manager.cgi_pid, NULL, 0);
		manager.cgi_pid = -1;
	}

	manager.cgi_state = FINISHED;
	manager.to_cgi_fd = -1;
	manager.from_cgi_fd = -1;
}
