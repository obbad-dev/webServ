#pragma once

#include <string>
#include <set>
#include <vector>
#include <map>
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
    pair<int, string> returnPair;
    bool hasReturnFlag;
    bool uploadEnabled;
    string uploadPath;
    bool hasUploadFlag;
    map<string, string> cgiPass;
    

public:
    LocationConf();
    ~LocationConf();

    void setPath(const string &token);
    void setAllowMethods(const vector<string> &methods);
    void setRoot(const string &rootPath);
    void setAutoindex(const string &token);
    void setIndex(const vector<string> &indexFiles);
    void setReturn(const string &path, const string &status);
    void setUpload(const string &path);
    void setEnableUpload(const string &token);
    void setCgiPass(const string &extension, const string &path);

    const string &getRoot() const;
    const bool &rootIsSet() const;
    const string &getPath() const;
    const set<string> &getAllowMethods() const;
    const bool &hasAutoindex() const;
    const vector<string> &getIndex() const;
    const bool &indexIsSet() const;
    const pair<int, string> &getReturn() const;
    const bool &hasReturn() const;
    const bool &uploadEnabledStatus() const;
    const string &getUploadPath() const;
    const bool &uploadIsSet() const;
    const map<string, string> &getCgiPass() const;
};
