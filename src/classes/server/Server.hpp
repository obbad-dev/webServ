#pragma once

#include <string>
#include <vector>
#include <stdint.h>
#include <map>
#include "LocationConf.hpp"

const size_t KB_MULTIPLIER = 1024ULL;
const size_t MB_MULTIPLIER = 1024ULL * 1024ULL;
const size_t GB_MULTIPLIER = 1024ULL * 1024ULL * 1024ULL;

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
        vector<Listen> listens;
        string _root;
        vector<string> index;
        uint64_t client_max_body_size;
        bool has_set_client_max_body_size;
        map<int, string> errors_page;
        vector<LocationConf> locations;
        string server_name;

    public:
        Server();
        ~Server();

        void setListen(const string& );
        void setRoot(const string& );
        void setIndex(const vector<string>& );
        void setClientMaxBodySize(const string& );
        void setErrorsPages(vector<string>& );
        void pushLocation(LocationConf& location);
        void setServerName(const string& name);

        const string &getServerName() const;
        const vector<Listen> &getListens() const;
        const string &getRoot() const;
        const vector<string>& getIndex() const;
        const uint64_t& getClientMaxBodySize() const;
        const bool& hasSetClientMaxBodySize() const;
        const map<int, string>& getErrorsPages() const;
        const vector<LocationConf> &getLocations() const;
        
};