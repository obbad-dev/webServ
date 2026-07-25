#include "ServerSide.hpp"
#include <iostream>
using namespace std;

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
        signal(SIGINT, sighandler);
        signal(SIGPIPE, SIG_IGN);
        string fileName = "resources/configFiles/default.conf";
        if (ac == 2)
            fileName = av[1];
        ParseConfig parseConfig = ParseConfig(fileName);
        ServerSide srv = ServerSide(parseConfig.getServers());
        srv.setup();
    }
    catch(const exception& e)
    {
        cerr << e.what() << endl;
        return 1;
    }
}
