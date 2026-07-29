#include "ServerSide.hpp"
#include "HttpException.hpp"
#include "helperFunc.hpp"
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
                throw runtime_error("Socket failed to open");

            int opt = 1;
            if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
                throw runtime_error("Setsockopt failed");

            if (fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1)
                throw runtime_error("Fcntl for server failed");

            if (fcntl(sockfd, F_SETFD, FD_CLOEXEC) == -1)
                throw runtime_error("Fcntl FD_CLOEXEC for server failed");

            struct sockaddr_in s_addr;
            bzero(&s_addr, sizeof(s_addr));
            s_addr.sin_family = AF_INET;
            s_addr.sin_port = htons(tmp_listen[j].port);
            s_addr.sin_addr.s_addr = inet_addr(tmp_listen[j].ip.c_str());

            if (bind(sockfd, reinterpret_cast<sockaddr *>(&s_addr), sizeof(s_addr)) == -1)
                throw runtime_error("Bind failed");

            if (listen(sockfd, SOMAXCONN) == -1)
                throw runtime_error("listen failed");

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

void disconnect_client(int fd, FdManager &manager, map<int, int> &cgiToClient)
{
    cleanupCgi(manager, cgiToClient);
    ServerSide::remove_from_epoll(manager.epollFd, fd);
    close(fd);
}

void ServerSide::acceptNewConnections(int epoll_fd, int server_fd, FdManager &serverManager)
{
    while (1)
    {
        int clientfd = accept(server_fd, NULL, NULL);

        if (clientfd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            return;
        }

        if (fcntl(clientfd, F_SETFL, O_NONBLOCK) == -1)
            throw runtime_error("Fcntl for client failed");

        if (fcntl(clientfd, F_SETFD, FD_CLOEXEC) == -1)
            throw runtime_error("Fcntl FD_CLOEXEC for client failed");

        add_fd_to_epoll(epoll_fd, clientfd, EPOLLIN);

        fds.insert(std::make_pair(clientfd, FdManager(CLIENT, time(NULL), serverManager.blockServer, epoll_fd, serverManager.listen)));
    }
}

void ServerSide::handleClientInput(int epoll_fd, int client_fd, FdManager &manager)
{

    manager.lastActivity = time(NULL);
    HttpRequest &request = manager.request;
    HttpResponse &response = manager.response;

    try
    {
        if (!request.parseRequest(client_fd, manager))
        {
            disconnect_client(client_fd, manager, cgiToClient);
            fds.erase(client_fd);
            return;
        }
        if (request.isComplete())
        {
            string script_path;
            string interpreter_path;
            bool is_cgi = request.isCgi(script_path, interpreter_path, manager);

            if (is_cgi)
            {
                if (manager.cgi_state == NOT_FINISHED)
                    return;
                response.prepareCGI(manager, script_path, interpreter_path);
                cgiToClient[manager.from_cgi_fd] = client_fd;
                if (manager.stat_fd_to_cgi == NOT_FINISHED)
                    cgiToClient[manager.to_cgi_fd] = client_fd;
            }
            else if (!is_cgi)
            {
                response.buildStaticResponse(manager);
                change_epoll_event(epoll_fd, client_fd, EPOLLOUT);
            }
        }
    }
    catch (const HttpException &e)
    {
        response.buildErrorResponse(e, manager.blockServer);
        const string &httpVersion = request.getProtocolVersion().empty() ? "HTTP/1.1" : request.getProtocolVersion();
        response.serializeResponse(httpVersion);
        change_epoll_event(epoll_fd, client_fd, EPOLLOUT);
    }
}

void ServerSide::handleCgiEvent(int epoll_fd, int client_fd, int cgi_fd, uint32_t events, FdManager &manager)
{
    try
    {
        manager.response.excuteCGI(manager, cgi_fd, events);

        if (manager.stat_fd_to_cgi == FINISHED)
        {
            cgiToClient.erase(manager.to_cgi_fd);
        }
        if (manager.stat_fd_from_cgi == FINISHED)
        {
            cgiToClient.erase(manager.from_cgi_fd);
        }
        if (manager.cgi_state == FINISHED)
        {
            manager.response.parseCgiOutput();
            manager.response.serializeResponse(manager.request.getProtocolVersion());
            change_epoll_event(epoll_fd, client_fd, EPOLLOUT);
        }
    }
    catch (const HttpException &e)
    {
        cleanupCgi(manager, cgiToClient);
        manager.response.buildErrorResponse(e, manager.blockServer);
        manager.response.serializeResponse(manager.request.getProtocolVersion());
        change_epoll_event(epoll_fd, client_fd, EPOLLOUT);
    }
}

void ServerSide::handleClientOutput(int epoll_fd, int client_fd, FdManager &manager)
{
    int ret = manager.response.send_response(client_fd);
    if (ret == -1)
    {
        disconnect_client(client_fd, manager, cgiToClient);
        fds.erase(client_fd);
    }
    else if (ret == 1)
    {
        manager.lastActivity = time(NULL);
        if (!manager.request.isKeepAlive())
        {
            disconnect_client(client_fd, manager, cgiToClient);
            fds.erase(client_fd);
        }
        else
        {
            manager.reset();
            change_epoll_event(epoll_fd, client_fd, EPOLLIN);
        }
    }
}

void ServerSide::handleClientTimeouts()
{
    time_t currentTime = time(NULL);
    for (map<int, FdManager>::iterator it = fds.begin(); it != fds.end();)
    {
        if (it->second.type == CLIENT && (currentTime - it->second.lastActivity) > TIMEOUT)
        {
            disconnect_client(it->first, it->second, cgiToClient);
            map<int, FdManager>::iterator tmp = it++;
            fds.erase(tmp);
        }
        else
            it++;
    }
}

void ServerSide::communication_part()
{
    int epoll_fd = epoll_create1(EPOLL_CLOEXEC);

    if (epoll_fd == -1)
        throw runtime_error("Epoll creation failed");

    for (map<int, FdManager>::iterator it = fds.begin(); it != fds.end(); it++)
    {
        add_fd_to_epoll(epoll_fd, it->first, EPOLLIN);
    }

    struct epoll_event event_arr[1024];

    while (!sig)
    {
        int epoll_ready = epoll_wait(epoll_fd, event_arr, 1024, 1000);

        if (epoll_ready == -1)
        {
            if (errno == EINTR)
                continue;
            throw runtime_error("Epoll wait failed");
        }

        for (int i = 0; i < epoll_ready; i++)
        {
            int current_fd = event_arr[i].data.fd;

            map<int, int>::iterator cgi_it = cgiToClient.find(current_fd);
            int client_fd = (cgi_it != cgiToClient.end()) ? cgi_it->second : current_fd;

            map<int, FdManager>::iterator manager_it = fds.find(client_fd);
            if (manager_it == fds.end())
                continue;
            if (manager_it->second.type == SERVER)
            {
                acceptNewConnections(epoll_fd, current_fd, manager_it->second);
            }
            else if (cgi_it != cgiToClient.end())
            {
                handleCgiEvent(epoll_fd, client_fd, current_fd, event_arr[i].events, manager_it->second);
            }
            else if (event_arr[i].events & EPOLLIN)
            {
                handleClientInput(epoll_fd, client_fd, manager_it->second);
            }
            else if (event_arr[i].events & EPOLLOUT)
            {
                handleClientOutput(epoll_fd, client_fd, manager_it->second);
            }
        }
        handleClientTimeouts();
    }

    // Cleanup
    for (map<int, FdManager>::iterator it = fds.begin(); it != fds.end(); it++)
    {
        if (it->second.type == CLIENT)
            cleanupCgi(it->second, cgiToClient);
        remove_from_epoll(epoll_fd, it->first);
        close(it->first);
    }
    close(epoll_fd);
}

map<string, string> FdManager::extensions;

void ServerSide::setup()
{
    create_server_sock();

    init_extensions_map();

    communication_part();
}
