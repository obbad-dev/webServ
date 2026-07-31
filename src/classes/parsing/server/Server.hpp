#pragma once

#include <string>
#include <vector>
#include <stdint.h>
#include <map>
#include "LocationConf.hpp"

const size_t KB_MULTIPLIER = 1024ULL;
const size_t MB_MULTIPLIER = 1024ULL * 1024ULL;
const size_t GB_MULTIPLIER = 1024ULL * 1024ULL * 1024ULL;

struct Listen{
    long port;
    std::string ip;
    bool operator==(const Listen& other) const {
        return ip == other.ip && port == other.port;
    }
};


class Server
{
    private:
        std::vector<Listen> listens;
        std::string _root;
        std::vector<std::string> index;
        uint64_t client_max_body_size;
        bool has_set_client_max_body_size;
        std::map<int, std::string> errors_page;
        std::vector<LocationConf> locations;
        std::string server_name;

    public:
        Server();
        ~Server();

        void setListen(const std::string& );
        void setRoot(const std::string& );
        void setIndex(const std::vector<std::string>& );
        void setClientMaxBodySize(const std::string& );
        void setErrorsPages(std::vector<std::string>& );
        void pushLocation(LocationConf& location);
        void setServerName(const std::string& name);

        const std::string &getServerName() const;
        const std::vector<Listen> &getListens() const;
        const std::string &getRoot() const;
        const std::vector<std::string>& getIndex() const;
        const uint64_t& getClientMaxBodySize() const;
        const bool& hasSetClientMaxBodySize() const;
        const std::map<int, std::string>& getErrorsPages() const;
        const std::vector<LocationConf> &getLocations() const;
        std::vector <LocationConf>& getForModifyLocation();
};