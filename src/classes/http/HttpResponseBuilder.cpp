#include "HttpResponseBuilder.hpp"
#include "ServerSide.hpp"
#include "helperFunc.hpp"
#include <sstream>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <cstdlib>
#include <cstdio>
#include <fcntl.h>

void HttpResponseBuilder::build(FdManager &manager)
{
    HttpRequest &request = manager.request;
    HttpResponse &response = manager.response;
    const Server &server = manager.blockServer;

    try
    {
        // 1. Client max body size verification
        if (server.hasSetClientMaxBodySize() && request.getBodyContent().size() > server.getClientMaxBodySize())
        {
            throw HttpException(STATUS_PAYLOAD_TOO_LARGE);
        }

        // 2. Find matching location (Longest Prefix Match)
        const LocationConf *location = findLocation(request.getPath(), server);

        // 3. Handle Redirection (301 / 302)
        if (location && location->hasReturn())
        {
            pair<int, string> redir = location->getReturn();
            response.setStatusCode(redir.first);
            response.setMessage(HttpResponse::getDefaultStatusMessage(redir.first));
            response.setResponseHeader("Location", redir.second);
            response.setResponseBody("Redirecting...");
            response.serializeResponse(request.getProtocolVersion().empty() ? "HTTP/1.1" : request.getProtocolVersion());
            return;
        }

        // 4. Method Verification (405 Method Not Allowed)
        if (location)
        {
            const set<string> &allowed = location->getAllowMethods();
            if (allowed.find(request.getMethod()) == allowed.end())
            {
                response.buildErrorResponse(HttpException(STATUS_METHOD_NOT_ALLOWED), server);
                // Set Allow header
                string allowHeader;
                for (set<string>::const_iterator it = allowed.begin(); it != allowed.end(); ++it)
                {
                    if (it != allowed.begin())
                        allowHeader += ", ";
                    allowHeader += *it;
                }
                response.setResponseHeader("Allow", allowHeader);
                response.serializeResponse(request.getProtocolVersion().empty() ? "HTTP/1.1" : request.getProtocolVersion());
                return;
            }
        }

        // 5. Construct Physical Path
        string root = (location && location->rootIsSet()) ? location->getRoot() : server.getRoot();
        string physicalPath;
        if (!realPath(root, request.getPath(), physicalPath))
        {
            throw HttpException(STATUS_FORBIDDEN); // Escaping the root directory structure
        }

        // 6. File vs Directory Resolution
        struct stat pathStat;
        if (stat(physicalPath.c_str(), &pathStat) != 0)
        {
            throw HttpException(STATUS_NOT_FOUND);
        }

        // Case A: Directory
        if (S_ISDIR(pathStat.st_mode))
        {
            const vector<string> &indexes = (location && location->indexIsSet()) ? location->getIndex() : server.getIndex();
            bool indexFound = false;
            for (size_t i = 0; i < indexes.size(); ++i)
            {
                string testIndex = physicalPath;
                if (testIndex[testIndex.size() - 1] != '/')
                    testIndex += "/";
                testIndex += indexes[i];

                struct stat indexStat;
                if (stat(testIndex.c_str(), &indexStat) == 0 && S_ISREG(indexStat.st_mode))
                {
                    physicalPath = testIndex;
                    indexFound = true;
                    break;
                }
            }

            if (!indexFound)
            {
                // Check Autoindex
                if (location && location->hasAutoindex())
                {
                    string listing = generateDirectoryListing(physicalPath, request.getPath());
                    if (listing.empty())
                    {
                        throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
                    }
                    response.setStatusCode(200);
                    response.setMessage("OK");
                    response.setResponseHeader("Content-Type", "text/html");
                    response.setResponseHeader("Content-Length", intToString(listing.size()));
                    response.setResponseBody(listing);
                    response.serializeResponse(request.getProtocolVersion().empty() ? "HTTP/1.1" : request.getProtocolVersion());
                    return;
                }
                else
                {
                    throw HttpException(STATUS_FORBIDDEN);
                }
            }
        }

        // Re-read physicalPath stats after potential index resolution
        if (stat(physicalPath.c_str(), &pathStat) != 0)
        {
            throw HttpException(STATUS_NOT_FOUND);
        }

        if (!S_ISREG(pathStat.st_mode))
        {
            throw HttpException(STATUS_FORBIDDEN);
        }

        // Case B: File
        // 7. Check CGI extension
        string ext = "";
        size_t dotPos = physicalPath.rfind('.');
        if (dotPos != string::npos)
        {
            ext = physicalPath.substr(dotPos);
        }

        if (location)
        {
            const map<string, string> &cgiMap = location->getCgiPass();
            map<string, string>::const_iterator cgiIt = cgiMap.find(ext);
            if (cgiIt != cgiMap.end())
            {
                executeCGI(manager, physicalPath);
                return;
            }
        }

        // Static Content Handler
        if (request.getMethod() == "GET")
        {
            string body;
            if (!readBinaryFile(physicalPath, body))
            {
                throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
            }
            response.setStatusCode(200);
            response.setMessage("OK");
            response.setResponseHeader("Content-Type", getMimeType(physicalPath, "application/octet-stream"));
            response.setResponseHeader("Content-Length", intToString(body.size()));
            response.setResponseBody(body);
        }
        else if (request.getMethod() == "POST")
        {
            // Uploads handling
            if (location && location->uploadEnabledStatus())
            {
                string uploadDir = location->getUploadPath();
                if (uploadDir.empty())
                    uploadDir = ".";

                string filename = request.getPath();
                size_t pos = filename.rfind('/');
                if (pos != string::npos)
                    filename = filename.substr(pos + 1);

                if (filename.empty() || filename == "upload")
                {
                    filename = "upload_" + intToString(time(NULL));
                }

                string finalUploadPath = uploadDir;
                if (finalUploadPath.empty() || finalUploadPath[finalUploadPath.size() - 1] != '/')
                    finalUploadPath += "/";
                finalUploadPath += filename;

                ofstream outFile(finalUploadPath.c_str(), ios::out | ios::binary);
                if (!outFile.is_open())
                {
                    throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
                }
                const string &fileData = request.getBodyContent();
                outFile.write(fileData.data(), fileData.size());
                outFile.close();

                response.setStatusCode(201);
                response.setMessage("Created");
                response.setResponseHeader("Content-Type", "text/plain");
                string bodyContent = "File uploaded successfully: " + filename;
                response.setResponseHeader("Content-Length", intToString(bodyContent.size()));
                response.setResponseBody(bodyContent);
            }
            else
            {
                throw HttpException(STATUS_METHOD_NOT_ALLOWED);
            }
        }
        else if (request.getMethod() == "DELETE")
        {
            if (std::remove(physicalPath.c_str()) == 0)
            {
                response.setStatusCode(204);
                response.setMessage("No Content");
                response.setResponseBody("");
            }
            else
            {
                throw HttpException(STATUS_FORBIDDEN);
            }
        }

        response.serializeResponse(request.getProtocolVersion().empty() ? "HTTP/1.1" : request.getProtocolVersion());
    }
    catch (const HttpException &e)
    {
        response.buildErrorResponse(e, server);
        response.serializeResponse(request.getProtocolVersion().empty() ? "HTTP/1.1" : request.getProtocolVersion());
    }
    catch (const std::exception &e)
    {
        response.buildErrorResponse(HttpException(STATUS_INTERNAL_SERVER_ERROR), server);
        response.serializeResponse(request.getProtocolVersion().empty() ? "HTTP/1.1" : request.getProtocolVersion());
    }
}

const LocationConf* HttpResponseBuilder::findLocation(const string& requestPath, const Server& server)
{
    const LocationConf* bestMatch = NULL;
    size_t maxLen = 0;
    const vector<LocationConf>& locations = const_cast<Server&>(server).getLocations();

    for (size_t i = 0; i < locations.size(); ++i)
    {
        const string& locPath = locations[i].getPath();
        if (requestPath.find(locPath) == 0)
        {
            if (locPath.length() > maxLen)
            {
                maxLen = locPath.length();
                bestMatch = &locations[i];
            }
        }
    }
    return bestMatch;
}

string HttpResponseBuilder::generateDirectoryListing(const string &dirPath, const string &uriPath)
{
    DIR *dir = opendir(dirPath.c_str());
    if (!dir)
    {
        return "";
    }
    
    stringstream ss;
    ss << "<html><head><title>Index of " << uriPath << "</title></head><body>\n";
    ss << "<h1>Index of " << uriPath << "</h1><hr><ul>\n";
    
    if (uriPath != "/" && !uriPath.empty())
    {
        ss << "<li><a href=\"../\">../ (Parent Directory)</a></li>\n";
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        string name = entry->d_name;
        if (name == "." || name == "..")
            continue;
            
        string fullPath = dirPath;
        if (fullPath.empty() || fullPath[fullPath.size() - 1] != '/')
            fullPath += "/";
        fullPath += name;
        
        struct stat s;
        bool isDir = false;
        if (stat(fullPath.c_str(), &s) == 0)
        {
            if (S_ISDIR(s.st_mode))
            {
                isDir = true;
            }
        }
        
        string linkName = name;
        if (isDir)
            linkName += "/";
            
        ss << "<li><a href=\"" << linkName << "\">" << linkName << "</a></li>\n";
    }
    closedir(dir);
    ss << "</ul><hr></body></html>";
    return ss.str();
}

bool HttpResponseBuilder::readBinaryFile(const string& filepath, string& content)
{
    ifstream file(filepath.c_str(), ios::in | ios::binary | ios::ate);
    if (!file.is_open())
        return false;
    
    streamsize size = file.tellg();
    file.seekg(0, ios::beg);
    
    content.resize(size);
    if (file.read(&content[0], size))
        return true;
    return false;
}

void HttpResponseBuilder::executeCGI(FdManager &manager, const std::string &physicalPath)
{
    HttpRequest &request = manager.request;
    HttpResponse &response = manager.response;

    static int counter = 0;
    std::string inPath = "/tmp/cgi_in_" + intToString(counter);
    std::string outPath = "/tmp/cgi_out_" + intToString(counter++);

    int inFd = open(inPath.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666);
    int outFd = open(outPath.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (inFd < 0 || outFd < 0)
    {
        if (inFd >= 0) close(inFd);
        if (outFd >= 0) close(outFd);
        throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
    }

    const std::string &body = request.getBodyContent();
    if (!body.empty())
        write(inFd, body.data(), body.size());
    lseek(inFd, 0, SEEK_SET);

    std::string scriptName = request.getPath();
    std::string query;
    size_t qPos = scriptName.find('?');
    if (qPos != std::string::npos)
    {
        query = scriptName.substr(qPos + 1);
        scriptName = scriptName.substr(0, qPos);
    }

    std::vector<std::string> env;
    env.push_back("REQUEST_METHOD=" + request.getMethod());
    env.push_back("SCRIPT_FILENAME=" + physicalPath);
    env.push_back("SCRIPT_NAME=" + scriptName);
    env.push_back("QUERY_STRING=" + query);
    env.push_back("SERVER_PROTOCOL=HTTP/1.1");
    env.push_back("GATEWAY_INTERFACE=CGI/1.1");

    const std::map<std::string, std::string> &headers = request.getHeaders();
    std::map<std::string, std::string>::const_iterator it = headers.find("content-length");
    if (it != headers.end())
        env.push_back("CONTENT_LENGTH=" + it->second);
    it = headers.find("content-type");
    if (it != headers.end())
        env.push_back("CONTENT_TYPE=" + it->second);

    std::vector<char*> envp;
    for (size_t i = 0; i < env.size(); ++i)
        envp.push_back(const_cast<char*>(env[i].c_str()));
    envp.push_back(NULL);

    pid_t pid = fork();
    if (pid == -1)
    {
        close(inFd); close(outFd);
        std::remove(inPath.c_str()); std::remove(outPath.c_str());
        throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
    }

    if (pid == 0)
    {
        dup2(inFd, STDIN_FILENO);
        dup2(outFd, STDOUT_FILENO);
        close(inFd);
        close(outFd);

        std::string dir = physicalPath.substr(0, physicalPath.find_last_of('/'));
        if (!dir.empty())
            chdir(dir.c_str());

        char *argv[3] = {
            const_cast<char*>("python3"),
            const_cast<char*>(physicalPath.c_str()),
            NULL
        };
        execve("/usr/bin/python3", argv, &envp[0]);
        exit(127);
    }

    int status;
    waitpid(pid, &status, 0);
    close(inFd);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        close(outFd);
        std::remove(inPath.c_str()); std::remove(outPath.c_str());
        throw HttpException(STATUS_BAD_GATEWAY);
    }

    std::string output;
    char buf[4096];
    lseek(outFd, 0, SEEK_SET);
    ssize_t n;
    while ((n = read(outFd, buf, sizeof(buf))) > 0)
        output.append(buf, n);
    close(outFd);
    std::remove(inPath.c_str());
    std::remove(outPath.c_str());

    size_t sep = output.find("\r\n\r\n");
    size_t sepLen = 4;
    if (sep == std::string::npos)
    {
        sep = output.find("\n\n");
        sepLen = 2;
    }

    std::string cgiBody = (sep == std::string::npos) ? output : output.substr(sep + sepLen);

    response.setStatusCode(200);
    response.setMessage("OK");
    response.setResponseHeader("Content-Length", intToString(cgiBody.size()));
    response.setResponseBody(cgiBody);
    response.serializeResponse(request.getProtocolVersion().empty() ? "HTTP/1.1" : request.getProtocolVersion());
}

string HttpResponseBuilder::getMimeType(const string &path, const string &defaultMime)
{
    string substring;
    size_t pos = path.rfind(".");
    if (pos != string::npos)
        substring = path.substr(pos);
    else
        return defaultMime;

    map<string, string>::iterator it = FdManager::extensions.find(substring);
    if (it != FdManager::extensions.end())
        return it->second;
    else
        return defaultMime;
}

string HttpResponseBuilder::intToString(int number)
{
    stringstream ss;
    ss << number;
    return ss.str();
}
