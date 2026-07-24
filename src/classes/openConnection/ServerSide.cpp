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

void ServerSide::acceptNewConnections(int epoll_fd, int server_fd, FdManager& serverManager)
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

        add_fd_to_epoll(epoll_fd, clientfd, EPOLLIN);

        fds.insert(std::make_pair(clientfd, FdManager(CLIENT, time(NULL), serverManager.blockServer, epoll_fd, serverManager.listen)));
    }
}

void ServerSide::handleClientInput(int epoll_fd, int client_fd, map<int, FdManager>::iterator& it)
{
    // Update activity timer to prevent timeout
    it->second.lastActivity = time(NULL);

    try {
        // Read and parse the incoming HTTP request
        if (it->second.request.parseRequest(client_fd) == false)
        {
            // Parsing failed or client disconnected
            cout << "fifth remove epoll\n";
            disconnect_client(client_fd, it->second);
            fds.erase(client_fd);
            return;
        }

        // Check if the request is fully received and parsed
        if (it->second.request.isComplete())
        {
            string cgi_path = ""; 
            bool is_cgi = it->second.request.isCgi(it->second.blockServer, cgi_path);
            
            if (is_cgi) {
                // Prepare pipes and fork the CGI process
                it->second.response.prepareCGI(it->second, cgi_path);
        
                // Map both ends of the CGI pipe back to this client's file descriptor
				if (it->second.to_cgi_fd != -1)
                	cgiToClient[it->second.to_cgi_fd] = client_fd;
				if (it->second.from_cgi_fd != -1)
                	cgiToClient[it->second.from_cgi_fd] = client_fd;
            } else {
                // It's a static file request. We need to build the static response here.
                it->second.response.buildStaticResponse(it->second.request, it->second.blockServer);
                it->second.response.serializeResponse(it->second.request.getProtocolVersion());
				it->second.request.resetRequest();

                // Once the response is built, we wait for the socket to be writable
                change_epoll_event(epoll_fd, client_fd, EPOLLOUT);
            }
        }
    } catch (const HttpException& e) {
        // Handle any errors thrown during parsing or building the response
        it->second.response.buildErrorResponse(e, it->second.blockServer);
        it->second.response.serializeResponse(it->second.request.getProtocolVersion());
        change_epoll_event(epoll_fd, client_fd, EPOLLOUT);
    }
}

void ServerSide::handleCgiEvent(int epoll_fd, int client_fd, int cgi_fd, uint32_t events, map<int, FdManager>::iterator& it)
{
    try {
        // Execute CGI read or write operations based on the event and triggered FD
        it->second.response.excuteCGI(it->second, cgi_fd, events);
		
		if (it->second.stat_fd_to_cgi == FINISHED ) {
			cgiToClient.erase(it->second.to_cgi_fd);
			it->second.to_cgi_fd = -1;
		}
		if (it->second.stat_fd_from_cgi == FINISHED) {
			cgiToClient.erase(it->second.from_cgi_fd);
			it->second.from_cgi_fd = -1;
		}
		
        // If the CGI process has completely finished (both pipes closed and process reaped)
        if (it->second.cgi_state == FINISHED) {
            // 1. Parse the raw CGI output to extract headers and the real body
            it->second.response.parseCgiOutput();
            
            // 2. Build the final HTTP response string
            it->second.response.serializeResponse(it->second.request.getProtocolVersion());
            
            // 3. Now we tell epoll we are ready to send it to the client
            change_epoll_event(epoll_fd, client_fd, EPOLLOUT);
        }
    } catch (const HttpException& e) {
        // If the CGI process fails, build an HTTP error response (e.g. 500)
        it->second.response.buildErrorResponse(e, it->second.blockServer);
        cgiToClient.erase(it->second.to_cgi_fd);
        cgiToClient.erase(it->second.from_cgi_fd);
		it->second.stat_fd_to_cgi = NOT_FINISHED;
		it->second.stat_fd_from_cgi = NOT_FINISHED;
		it->second.to_cgi_fd = -1;
		it->second.from_cgi_fd = -1;
		it->second.cgi_state = NOT_FINISHED;
		it->second.response.serializeResponse(it->second.request.getProtocolVersion());
		change_epoll_event(epoll_fd, client_fd, EPOLLOUT);
    }
}

void ServerSide::handleClientOutput(int epoll_fd, int client_fd, map<int, FdManager>::iterator& it)
{
    // Attempt to send the serialized HTTP response over the socket
    int ret = it->second.response.send_response(client_fd);
	// cout << "Sent response to client_fd: " << client_fd << ", ret: " << ret << endl;
    
    if (ret == -1)
    {
        // Connection error while sending
        cout << "sixth remove epoll\n";
        disconnect_client(client_fd, it->second);
        fds.erase(client_fd);
    }
    else if (ret == 1)
    {
        // The entire response was successfully sent
        it->second.lastActivity = time(NULL);
        
        // Switch back to listening for new requests on this connection (Keep-Alive)
        change_epoll_event(epoll_fd, client_fd, EPOLLIN);
        if (!it->second.request.isKeepAlive())
        {
            // If the client requested Connection: close, disconnect immediately
            cout << "seventh remove epoll\n";
            disconnect_client(client_fd, it->second);
            fds.erase(it);
        }
    }
    // If ret == 0, it means EAGAIN (socket buffer full). We do nothing and wait for next EPOLLOUT.
}

void ServerSide::handleClientTimeouts()
{
    time_t currentTime = time(NULL);
    for (map<int, FdManager>::iterator it = fds.begin(); it != fds.end(); )
    {
        // Check if the connection has been idle longer than the allowed TIMEOUT
        if (it->second.type == CLIENT && (currentTime - it->second.lastActivity) > TIMEOUT)
        {
            cout << "eighth remove epoll\n";
            disconnect_client(it->first, it->second);
            map<int, FdManager>::iterator tmp = it++;
            fds.erase(tmp);
        }
        else
        {
            it++;
        }
    }
}

void ServerSide::communication_part()
{
    int epoll_fd = epoll_create(1);

    if (epoll_fd == -1)
        throw runtime_error("Epoll creation failed");

    // Add all server listening sockets to epoll
    for (map<int, FdManager>::iterator it = fds.begin(); it != fds.end(); it++)
    {
        add_fd_to_epoll(epoll_fd, it->first, EPOLLIN);
    }

    struct epoll_event event_arr[1024];

    while (true)
    {
        // Wait for an event on any of the monitored file descriptors (timeout 1000ms)
        int epoll_ready = epoll_wait(epoll_fd, event_arr, 1024, 1000);

        if (epoll_ready == -1)
            throw runtime_error("Epoll wait failed");
        for (int i = 0; i < epoll_ready; i++)
        {
            int current_fd = event_arr[i].data.fd;
            
            // Check if the triggered fd is a CGI pipe. If so, map it to its client connection.
            map<int, int>::iterator cgi_it = cgiToClient.find(current_fd);
            int client_fd = (cgi_it != cgiToClient.end()) ? cgi_it->second : current_fd;
            
            // Find the state manager for this connection
            map<int, FdManager>::iterator manager_it = fds.find(client_fd);
            if (manager_it == fds.end())
				continue;
            if (manager_it->second.type == SERVER)
            {
                // Event is on the server listening socket: a new client is connecting
                acceptNewConnections(epoll_fd, current_fd, manager_it->second);
            }
            else if (cgi_it != cgiToClient.end())
            {
				cout << "CGI event on client_fd: " << client_fd << ", cgi_fd: " << current_fd << endl;
                // Event is on a CGI pipe (read or write is ready)
                handleCgiEvent(epoll_fd, client_fd, current_fd, event_arr[i].events, manager_it);
            }
            else if (event_arr[i].events & EPOLLIN)
            {
				// cout << "EPOLLIN event on client_fd: " << client_fd << endl;
                // Event is on a client socket, and it is ready to be read
                handleClientInput(epoll_fd, client_fd, manager_it);
            }
            else if (event_arr[i].events & EPOLLOUT)
            {
				// cout << "connection from fd: " << client_fd << " and keep-alive: " << manager_it->second.request.isKeepAlive() << "\n";
				// cout << "EPOLLOUT event on client_fd: " << client_fd << endl;
                // Event is on a client socket, and it is ready to be written to
                handleClientOutput(epoll_fd, client_fd, manager_it);
				// cout << "After handleClientOutput, client_fd: " << client_fd << endl;
            }
			// sleep (1);
        }
        
        // Regularly check for inactive clients and disconnect them
        handleClientTimeouts();
    }
}

map<string, string> FdManager::extensions;

void ServerSide::setup()
{
    create_server_sock();

    init_extensions_map();

    communication_part();
}

