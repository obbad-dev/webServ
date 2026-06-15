#pragma once

#include <string>
#include <set>
#include <vector>
using namespace std;

class LocationConf
{
private:
    string path;
    set<string> allowMethods;
    bool setAllowMethodsFlag;

public:
    LocationConf();
    ~LocationConf();

    void setPath(const string &token);
    const string &getPath() const;
    void setAllowMethods(const vector<string> &methods);
    const set<string> &getAllowMethods() const;
};

