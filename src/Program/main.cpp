#include "ServerSide.hpp"

int main (int ac, char *av[])
{
    try
    {
        string fileName = "resources/default.conf";
       if (ac == 2)
            fileName = av[1];
        ParseConfig parseConfig = ParseConfig(fileName);
        parseConfig.tokenize();
        parseConfig.parse();
        parseConfig.debug();
        ServerSide srv = ServerSide(parseConfig);
        srv.setup();
        // delete parseConfig;
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

