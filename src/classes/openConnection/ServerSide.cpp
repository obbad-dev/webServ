#include "ServerSide.hpp"
#include "HttpException.hpp"
ServerSide::ServerSide(const vector<Server> &servers) : servers(servers)
{
}

ServerSide::~ServerSide()
{
}

void init_extensions_map()
{
    FdManager::extensions[".html"] = "text/html";
    FdManager::extensions[".css"] = "text/css";
    FdManager::extensions[".js"] = "application/javascript";
    FdManager::extensions[".jpg"] = "image/jpeg";
    FdManager::extensions[".jpeg"] = "image/jpeg";
    FdManager::extensions[".png"] = "image/png";
    FdManager::extensions[".gif"] = "image/gif";
    FdManager::extensions[".ico"] = "image/x-icon";
    FdManager::extensions[".txt"] = "text/plain";
    FdManager::extensions[".pdf"] = "application/pdf";
}


void ServerSide::create_server_sock()
{
    for (size_t i = 0; i < servers.size(); i++)
    {
        const vector<Listen> &tmp_listen = servers[i].getListens();

        for (size_t j = 0; j < tmp_listen.size(); j++)
        {
            int sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd == -1)
                throw runtime_error("Socket failed to open"); // close previous files when error occurs

            int opt = 1;
            if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
                throw runtime_error("Setsockopt failed");

            if (fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1)
                throw runtime_error("Fcntl for server failed");

            struct sockaddr_in s_addr;
            bzero(&s_addr, sizeof(s_addr)); // replace with our bzero
            s_addr.sin_family = AF_INET;
            s_addr.sin_port = htons(tmp_listen[j].port);
            s_addr.sin_addr.s_addr = inet_addr(tmp_listen[j].ip.c_str()); // inet_addr not allowed

            if (bind(sockfd, reinterpret_cast<sockaddr*>(&s_addr), sizeof(s_addr)) == -1)
                throw runtime_error("Bind failed"); // close previous files when error occurs

            if (listen(sockfd, SOMAXCONN) == -1)
                throw runtime_error("listen failed"); // close previous files when error occurs

            fds.insert(std::make_pair(sockfd, FdManager(SERVER, time(NULL), servers[i], opt, tmp_listen[j])));
        }
    }
}

void ServerSide::add_fd_to_epoll(int epoll_fd, int fd, uint32_t events)
{
    struct epoll_event ev;

    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1)
        throw runtime_error("Epoll_ctl_add failed");
}

void ServerSide::remove_from_epoll(int epoll_fd, int fd)
{
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) == -1)
        throw runtime_error("Epoll_ctl_del failed");
}

void ServerSide::change_epoll_event(int epoll_fd, int fd, uint32_t events)
{
    struct epoll_event ev;

    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1)
        throw runtime_error("Epoll_ctl_mod failed");
}

void disconnect_client(int fd, FdManager &manager)
{
    ServerSide::remove_from_epoll(manager.epollFd, fd);
    close (fd);
}

void ServerSide::communication_part()
{
    int epoll_fd = epoll_create(1);

    if (epoll_fd == -1)
        throw runtime_error("Epoll creation failed");

    for (map<int, FdManager>::iterator it = fds.begin(); it != fds.end(); it++)
    {
        add_fd_to_epoll(epoll_fd, it->first, EPOLLIN);
    }

    struct epoll_event event_arr[1024];

    while (true)
    {
        int epoll_ready = epoll_wait(epoll_fd, event_arr, 1024, 1000);

        if (epoll_ready == -1)
            throw runtime_error("Epoll wait failed");
        for (int i = 0; i < epoll_ready; i++)
        {
            int current_fd = event_arr[i].data.fd;
            map<int, int>::iterator cgi_it = cgiToClient.find(current_fd);
            int client_fd = (cgi_it != cgiToClient.end()) ? cgi_it->second : current_fd;
            
            map<int, FdManager>::iterator it = fds.find(client_fd);
		
            if (it == fds.end()) continue;

            if (it->second.type == SERVER)
            {
                while (1)
                {
                    int clientfd = accept(it->first, NULL, NULL);

                    if (clientfd == -1)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;
                    }

                    if (fcntl(clientfd, F_SETFL, O_NONBLOCK) == -1)
                        throw runtime_error("Fcntl for client failed");

                    add_fd_to_epoll(epoll_fd, clientfd, EPOLLIN);

                    fds.insert(std::make_pair(clientfd, FdManager(CLIENT, time(NULL), it->second.blockServer, epoll_fd, it->second.listen)));
                }
            }
            else
            {
                if (event_arr[i].events & EPOLLIN)
                {
                    it->second.lastActivity = time(NULL);

                    if (it->second.request.isComplete())
                    {
                        // TODO: Determine if this request is a CGI request and get the CGI path.
                        bool is_cgi = false; // Replace with your actual CGI check
                        string cgi_path = ""; // Replace with your actual CGI path getter
                        
                        if (is_cgi) {
                            it->second.response.prepareCGI(it->second, cgi_path);
                            cgiToClient[it->second.to_cgi_fd] = it->first;
                            cgiToClient[it->second.from_cgi_fd] = it->first;
                        } else {
                            change_epoll_event(epoll_fd, it->first, EPOLLOUT);
                        }
                    }
					else if (it->second.request.parseRequest(it->first) == false)
                    {
                        disconnect_client(it->first, it->second);
                        fds.erase(it);
                        continue;
                    }
                }
                else if (cgi_it != cgiToClient.end())
                {
                    // This event is for a CGI pipe (to_cgi_fd or from_cgi_fd)
                    try {
                        it->second.response.excuteCGI(it->second, current_fd, event_arr[i].events);
                        if (it->second.cgi_state == FINISHED) {
                            // CGI is done, clean up mappings and set client socket to EPOLLOUT
                            cgiToClient.erase(it->second.to_cgi_fd);
                            cgiToClient.erase(it->second.from_cgi_fd);
                            change_epoll_event(epoll_fd, it->first, EPOLLOUT);
                        }
                    } catch (const HttpException& e) {
                        it->second.response.buildErrorResponse(e, it->second.blockServer);
                        cgiToClient.erase(it->second.to_cgi_fd);
                        cgiToClient.erase(it->second.from_cgi_fd);
                        change_epoll_event(epoll_fd, it->first, EPOLLOUT);
                    }
                }
                else if (event_arr[i].events & EPOLLOUT)
                {
                    int ret = it->second.response.send_response(it->first);
                    if (ret == -1)
                    {
                        disconnect_client(it->first, it->second);
                        fds.erase(it);
                        continue;
                    }
                    else if (ret == 1)
                    {
                        it->second.lastActivity = time(NULL);
                        change_epoll_event(epoll_fd, it->first, EPOLLIN);
                    }
                    if (!it->second.request.isKeepAlive())
                    {
                        disconnect_client(it->first, it->second);
                        fds.erase(it);
                    }
                }
            }
        }
        {
            time_t currentTime = time(NULL);
            for (map<int, FdManager>::iterator it = fds.begin(); it != fds.end(); )
            {
                if (it->second.type == CLIENT && (currentTime - it->second.lastActivity) > TIMEOUT)
                {
                    // cout << "Disconnecting client with fd = " << it->first << " because timeout = " << (currentTime - it->second.lastActivity) << endl;
                    disconnect_client(it->first, it->second);
                    map<int, FdManager>::iterator tmp = it++;
                    fds.erase(tmp);
                }
                else
                    it++;
            }
        }
    }
}

map<string, string> FdManager::extensions;

void ServerSide::setup()
{
    create_server_sock();

    init_extensions_map();

    communication_part();
}

