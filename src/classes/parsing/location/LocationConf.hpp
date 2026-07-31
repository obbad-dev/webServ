#pragma once

#include <string>
#include <set>
#include <vector>
#include <map>
#include <stdint.h>



class LocationConf
{
private:
    std::string path;
    std::set<std::string> allowMethods;
    bool setAllowMethodsFlag;
    std::string root;
    bool setRootFlag;
    bool autoindex;
    std::vector<std::string> index;
    bool hasIndexFlag;
    std::pair<int, std::string> returnPair;
    bool hasReturnFlag;
    bool uploadEnabled;
    std::string uploadPath;
    bool hasUploadFlag;
    std::pair<std::string, std::string> cgiPass;
	bool hasCgiPassFlag;
	bool hasClientMaxBodySizeFlag;
	uint64_t clientMaxBodySize;

public:
    LocationConf();
    ~LocationConf();

    void setPath(const std::string &token);
    void setAllowMethods(const std::vector<std::string> &methods);
    void setRoot(const std::string &rootPath);
    void setAutoindex(const std::string &token);
    void setIndex(const std::vector<std::string> &indexFiles);
    void setReturn(const std::string &path, const std::string &status);
    void setUpload(const std::string &path);
    void setEnableUpload(const std::string &token);
    void setCgiPass(const std::string &extension, const std::string &ineterpreter);
	void setClientMaxBodySize(const std::string &token);

    const std::string &getRoot() const;
    const bool &rootIsSet() const;
    const std::string &getPath() const;
    const std::set<std::string> &getAllowMethods() const;
    const bool &hasAutoindex() const;
    const std::vector<std::string> &getIndex() const;
    const bool &indexIsSet() const;
    const std::pair<int, std::string> &getReturn() const;
    const bool &hasReturn() const;
    const bool &uploadEnabledStatus() const;
    const std::string &getUploadPath() const;
    const bool &uploadIsSet() const;
    const std::pair<std::string, std::string> &getCgiPass() const;
    const bool &hasCgiPass() const;
	const bool &hasClientMaxBodySize() const;
	const uint64_t &getClientMaxBodySize() const;
    bool operator==(const LocationConf &other) const;
};
