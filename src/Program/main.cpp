#include "ServerSide.hpp"
#include <iostream>

int sig = 0;

void sighandler(int s)
{
    (void)s;
    sig = 1;
}

int main (int ac, char *av[])
{
    try
    {
        if (ac > 2)
        {
            std::cerr << "the program can be executed only with ./webserv or ./webserv configFile\n";
            return 1;
        }
        signal(SIGINT, sighandler);
        signal(SIGPIPE, SIG_IGN);
        std::string fileName = "resources/configFiles/default.conf";
        if (ac == 2)
            fileName = av[1];
        ParseConfig parseConfig = ParseConfig(fileName);
        ServerSide srv = ServerSide(parseConfig.getServers());
        srv.setup();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
