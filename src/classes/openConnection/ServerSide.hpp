#pragma once
using namespace std;

#include <cerrno>
#include <string>
#include <sys/epoll.h>

#include <arpa/inet.h> //not allowed
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <csignal>
#include <unistd.h>

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "ParseConfig.hpp"

#define TIMEOUT 120

enum CONN_TYPE { SERVER, CLIENT };
enum STATCGI { NOT_FINISHED, FINISHED };

extern int sig;

struct FdManager {
  Listen listen;
  time_t lastActivity;
  CONN_TYPE type;
  HttpRequest request;
  HttpResponse response;
  const Server &blockServer;
  std::vector<std::string>
      env_vars; // Array to hold environment variables for CGI

  int epollFd;
  static map<string, string> extensions;

  int to_cgi_fd;
  int from_cgi_fd;
  pid_t cgi_pid;
  STATCGI stat_fd_to_cgi;
  STATCGI stat_fd_from_cgi;
  STATCGI cgi_state;
  size_t cgi_bytes_written;

  FdManager(CONN_TYPE _type, time_t _lastActivity, const Server &_blockServer,
            int &_epollFd, const Listen &_listen)
      : listen(_listen), blockServer(_blockServer), epollFd(_epollFd) {
    type = _type;
    lastActivity = _lastActivity;
    cgi_state = NOT_FINISHED;
	stat_fd_to_cgi = NOT_FINISHED;
	stat_fd_from_cgi = NOT_FINISHED;
    cgi_bytes_written = 0;
    to_cgi_fd = -1;
    from_cgi_fd = -1;
    // pid_t cgi_pid = -1;
  }

  void reset() {
	// lastActivity = time(NULL);
	request.resetRequest();
	response.resetObjectResponse();
	cgi_state = NOT_FINISHED;
	to_cgi_fd = -1;
	from_cgi_fd = -1;
	stat_fd_to_cgi = NOT_FINISHED;
	stat_fd_from_cgi = NOT_FINISHED;
	cgi_bytes_written = 0;
  }
};

class ServerSide {
private:
  const vector<Server> &servers;
  map<int, FdManager> fds;
  map<int, int> cgiToClient;
  map<int, HttpRequest> httpRequests;

  // void debug();
  void acceptNewConnections(int epoll_fd, int server_fd,
                            FdManager &serverManager);
  void handleClientInput(int epoll_fd, int client_fd,
                         map<int, FdManager>::iterator &it);
  void handleCgiEvent(int epoll_fd, int client_fd, int cgi_fd, uint32_t events,
                      map<int, FdManager>::iterator &it);
  void handleClientOutput(int epoll_fd, int client_fd,
                          map<int, FdManager>::iterator &it);
  void handleClientTimeouts();

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
