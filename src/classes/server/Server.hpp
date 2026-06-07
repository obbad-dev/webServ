#pragma once

#include <string>
using namespace std;

class Server
{
private:
    int _port;
    string _root;

public:
    Server();
    ~Server();

    void setPort(string port);
    void setRoot(string root);

    const int &getPort() const;
    const string &getRoot() const;
};
