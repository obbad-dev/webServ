#pragma once

#include <string>

using namespace std;

class LocationConf
{
private:
    string path;

public:
    LocationConf();
    ~LocationConf();

    void setPath(const string &token);
    const string &getPath() const;
};

