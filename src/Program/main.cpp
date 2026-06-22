#include "ServerSide.hpp"
using namespace std;
#include <iostream>

int main (int ac, char *av[])
{
    try
    {
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

