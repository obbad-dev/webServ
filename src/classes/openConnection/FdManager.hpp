#pragma once

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "ParseConfig.hpp"

enum CONN_TYPE
{
  SERVER,
  CLIENT
};
enum STATCGI
{
  NOT_FINISHED,
  FINISHED
};

struct FdManager
{
    Listen listen;
    time_t lastActivity;
    CONN_TYPE type;
    HttpRequest request;
    HttpResponse response;
    const Server &blockServer;
    std::vector<std::string> env_vars;
    LocationConf *location;
    std::string target_path;

    int epollFd;
    static std::map<std::string, std::string> extensions;

    int to_cgi_fd;
    int from_cgi_fd;
    pid_t cgi_pid;
    STATCGI stat_fd_to_cgi;
    STATCGI stat_fd_from_cgi;
    STATCGI cgi_state;
    size_t cgi_bytes_written;
    size_t client_max_body_size;

    FdManager(CONN_TYPE _type, time_t _lastActivity, const Server &_blockServer,
        int &_epollFd, const Listen &_listen);

    void reset();
};
