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
    string root;
    bool setRootFlag;
    bool autoindex;
    vector<string> index;
    bool hasIndexFlag;

public:
    LocationConf();
    ~LocationConf();

    void setPath(const string &token);
    void setAllowMethods(const vector<string> &methods);
    void setRoot(const string &rootPath);
    void setAutoindex(const string &token);
    void setIndex(const vector<string> &indexFiles);

    const string &getRoot() const;
    const bool &rootIsSet() const;
    const string &getPath() const;
    const set<string> &getAllowMethods() const;
    const bool &hasAutoindex() const;
    const vector<string> &getIndex() const;
    const bool &indexIsSet() const;
};
