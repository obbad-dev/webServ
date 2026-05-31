#include "ParseConfig.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

class ServerSide
{
    private:
        ParseConfig& _config;
        
    public:
        ServerSide(ParseConfig& config);
        void setup();
        ~ServerSide();
};

