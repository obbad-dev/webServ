#include <vector>
#include <fstream>
#include <sstream>
#include <stack>
#include <list>
#include "Server.hpp"

class ParseConfig
{
    private:
        string _configFile;
        vector<string> _tokens;
        vector<Server> servers;

        void parseServer(Server& server, size_t& i);
        size_t incrIdx(size_t &i);

    public:
        ParseConfig(string configFile);
        void tokenize();
        void parse();
        const vector<Server>& getSrvers() const;

        void debug(){
            cout << "--- Parsed Configuration ---" << endl;
            for (size_t i  = 0; i < servers.size(); i++){
                cout << "Server [" << i << "]:" << endl;
                cout << "  Port: " << servers[i].getPort() << endl;
                cout << "  Root: " << servers[i].getRoot() << endl;
            }
            cout << "----------------------------" << endl;
        }
        ~ParseConfig();
};
