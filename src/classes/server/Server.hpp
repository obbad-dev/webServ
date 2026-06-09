#pragma once

#include <string>
#include <vector>
using namespace std;

struct Listen{
    long port;
    string ip;
    bool operator==(const Listen& other) const {
        return ip == other.ip && port == other.port;
    }
};

class Server
{
private:
    // int _port;
    vector<Listen> listens;
    string _root;

public:
    Server();
    ~Server();

    void setListen(string ip_port);
    void setRoot(string root);

    const vector<Listen> &getListens() const;
    const string &getRoot() const;
};
