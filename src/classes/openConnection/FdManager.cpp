#include "FdManager.hpp"

FdManager::FdManager(CONN_TYPE _type, time_t _lastActivity, const Server &_blockServer,
            int &_epollFd, const Listen &_listen)
      : listen(_listen), blockServer(_blockServer), epollFd(_epollFd)
{
    type = _type;
    lastActivity = _lastActivity;
    cgi_state = FINISHED;
    stat_fd_to_cgi = FINISHED;
    stat_fd_from_cgi = FINISHED;
    cgi_bytes_written = 0;
    to_cgi_fd = -1;
    from_cgi_fd = -1;
	cgi_pid = -1;
    location = NULL;
    client_max_body_size = static_cast<size_t>(blockServer.getClientMaxBodySize());
}

void FdManager::reset()
{
    request.resetRequest();
    response.resetObjectResponse();
    cgi_state = FINISHED;
    to_cgi_fd = -1;
    from_cgi_fd = -1;
	cgi_pid = -1;
    stat_fd_to_cgi = FINISHED;
    stat_fd_from_cgi = FINISHED;
    cgi_bytes_written = 0;
    target_path.clear();
    location = NULL;
    client_max_body_size = static_cast<size_t>(blockServer.getClientMaxBodySize());
}