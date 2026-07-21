#pragma once
using namespace std;

#include <string>
#include <sys/epoll.h>
#include <cerrno>

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h> //not allowed

#include "ParseConfig.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

#define TIMEOUT 30
enum CONN_TYPE {SERVER, CLIENT};
enum STATCGI {
	WRITE_DATA,
	READ_DATA,
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
	std::vector<std::string> env_vars;// Array to hold environment variables for CGI
	
    int &epollFd;
    static map<string, string> extensions;

	int to_cgi_fd;
	int from_cgi_fd;
	pid_t cgi_pid;
	bool cgi_finished;
	STATCGI cgi_state;
	size_t cgi_bytes_written;
	


    FdManager(CONN_TYPE _type, time_t _lastActivity, const Server &_blockServer, int &_epollFd, const Listen &_listen) : listen(_listen), blockServer(_blockServer), epollFd(_epollFd)
    {
        type = _type;
        lastActivity = _lastActivity;
        response.init_bytes_var();
		cgi_finished = false;
		cgi_state = FINISHED;
		cgi_bytes_written = 0;
		to_cgi_fd = -1;
		from_cgi_fd = -1;
		pid_t cgi_pid = -1;
    }
};

class ServerSide
{
private:
    const vector<Server> &servers;
    map<int, FdManager> fds;
    map<int, int> cgiToClient;
    map<int, HttpRequest> httpRequests;
    void debug();

public:
    ServerSide(const vector<Server> &servers);
    ~ServerSide();

    void setup();
    void create_server_sock();
    void communication_part();
    static void change_epoll_event(int epoll_fd, int fd, uint32_t events);
    static void add_fd_to_epoll(int epoll_fd, int fd, uint32_t events);
    static void remove_from_epoll(int epoll_fd, int fd);
};

