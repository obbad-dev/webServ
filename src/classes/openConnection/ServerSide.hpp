#pragma once
using namespace std;

#include <cerrno>
#include <string>
#include <sys/epoll.h>

#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "ParseConfig.hpp"
#include "FdManager.hpp"

#define TIMEOUT 60

extern int sig;

class ServerSide
{
private:
  const vector<Server> &servers;
  map<int, FdManager> fds;
  map<int, int> cgiToClient;
  map<int, HttpRequest> httpRequests;

  void acceptNewConnections(int epoll_fd, int server_fd, FdManager &serverManager);
  void handleClientInput(int epoll_fd, int client_fd, FdManager &manager);
  void handleCgiEvent(int epoll_fd, int client_fd, int cgi_fd, uint32_t events, FdManager &manager);
  void handleClientOutput(int epoll_fd, int client_fd, FdManager &manager);
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
  void resources_cleanup(int epoll_fd);
  void server_life_cycle(int epoll_fd);
};
