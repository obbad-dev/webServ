#include "ServerSide.hpp"
#include <cstring>

ServerSide::ServerSide(ParseConfig& config):_config(config)
{
    
}
ServerSide::~ServerSide(){}

void ServerSide::setup(){

    const Server &server = _config.getSrvers()[0];
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 1){
        perror("socket");
        throw runtime_error("");
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(server.getPort());

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        throw runtime_error("");
    }

    if (listen(server_fd, 10)){
        perror("listen");
        throw runtime_error("");
    }
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr* )& client_addr,&client_len);
    if (client_fd < 0)
    {
        perror("accept");
        throw runtime_error("");
    }
    char buffer[40096];
   std::memset(buffer, 0, 40096);
   int bytesRead = read(client_fd, buffer, 40095);
   if (bytesRead < 0) {
       perror("Read failed");
   } else {
       // 4. PRINT the request to your console!
       std::cout << "DEBUG: Received Request:\n" << std::endl;
       std::cout << buffer << std::endl;
       std::cout << "--------------------------" << std::endl;
   }

//    std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 12\r\n\r\n"+;
//    write(client_fd, response.c_str(), response.length());
   close(client_fd);

}

