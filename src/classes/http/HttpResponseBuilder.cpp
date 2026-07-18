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
                if (/*testIndex.empty() || */ testIndex[testIndex.size() - 1] != '/')
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
                executeCGI(manager, physicalPath, cgiIt->second);
                return;
            }
        }

        // Static Content Handler
        if (request.getMethod() == "GET" || request.getMethod() == "HEAD")
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
            if (request.getMethod() == "HEAD")
            {
                response.setResponseBody("");
            }
            else
            {
                response.setResponseBody(body);
            }
        }
        else if (request.getMethod() == "POST")
        {
            // Uploads handling
            if (location && location->uploadEnabledStatus())
            {
                string uploadDir = location->getUploadPath();
                if (uploadDir.empty())
                    uploadDir = ".";

                string filename = "";
                string boundary = "";
                map<string, string>::const_iterator ctIt = request.getHeaders().find("content-type");
                if (ctIt != request.getHeaders().end())
                {
                    size_t bPos = ctIt->second.find("boundary=");
                    if (bPos != string::npos)
                    {
                        boundary = ctIt->second.substr(bPos + 9);
                        if (boundary.size() >= 2 && boundary[0] == '"' && boundary[boundary.size() - 1] == '"')
                        {
                            boundary = boundary.substr(1, boundary.size() - 2);
                        }
                    }
                }

                string fileData;
                if (!boundary.empty())
                {
                    string boundaryStr = "--" + boundary;
                    size_t partStart = request.getBodyContent().find(boundaryStr);
                    if (partStart != string::npos)
                    {
                        size_t nextBoundary = request.getBodyContent().find(boundaryStr, partStart + boundaryStr.size());
                        if (nextBoundary != string::npos)
                        {
                            string part = request.getBodyContent().substr(partStart, nextBoundary - partStart);
                            size_t fnPos = part.find("filename=\"");
                            if (fnPos != string::npos)
                            {
                                size_t fnEnd = part.find("\"", fnPos + 10);
                                if (fnEnd != string::npos)
                                {
                                    filename = part.substr(fnPos + 10, fnEnd - (fnPos + 10));
                                }
                            }

                            size_t dataStart = part.find("\r\n\r\n");
                            if (dataStart != string::npos)
                            {
                                dataStart += 4;
                                size_t dataEnd = part.size();
                                if (dataEnd >= 2 && part[dataEnd - 2] == '\r' && part[dataEnd - 1] == '\n')
                                    dataEnd -= 2;
                                if (dataEnd > dataStart)
                                {
                                    fileData = part.substr(dataStart, dataEnd - dataStart);
                                }
                            }
                        }
                    }
                }

                if (filename.empty())
                {
                    stringstream ss;
                    ss << "upload_" << time(NULL) << "_" << rand() % 1000 << ".bin";
                    filename = ss.str();
                    fileData = request.getBodyContent();
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
                outFile.write(fileData.data(), fileData.size());
                outFile.close();

                response.setStatusCode(201);
                response.setMessage("Created");
                response.setResponseHeader("Content-Type", "text/html");
                string bodyContent = "<html><body><h1>201 Created</h1><p>File uploaded successfully as: " + filename + "</p></body></html>";
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
    ss << "<!DOCTYPE html>\n<html>\n<head>\n<title>Index of " << uriPath << "</title>\n";
    ss << "<style>\n";
    ss << "body { font-family: sans-serif; margin: 30px; background-color: #f7f9fa; color: #333; }\n";
    ss << "h1 { font-size: 24px; border-bottom: 1px solid #ccc; padding-bottom: 10px; }\n";
    ss << "ul { list-style-type: none; padding: 0; }\n";
    ss << "li { margin: 10px 0; font-size: 18px; }\n";
    ss << "a { color: #3498db; text-decoration: none; }\n";
    ss << "a:hover { text-decoration: underline; }\n";
    ss << "</style>\n</head>\n<body>\n";
    ss << "<h1>Index of " << uriPath << "</h1>\n<ul>\n";
    
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
    
    ss << "</ul>\n<hr>\n<p style=\"font-size: 12px; color: #999;\">webServ/1.0</p>\n</body>\n</html>";
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

void HttpResponseBuilder::executeCGI(FdManager &manager, const std::string &physicalPath, const std::string &interpreter)
{
    HttpRequest &request = manager.request;
    HttpResponse &response = manager.response;

    FILE *infile = tmpfile();
    FILE *outfile = tmpfile();
    if (!infile || !outfile)
    {
        if (infile) fclose(infile);
        if (outfile) fclose(outfile);
        throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
    }

    int in_fd = fileno(infile);
    int out_fd = fileno(outfile);

    if (!request.getBodyContent().empty())
    {
        write(in_fd, request.getBodyContent().data(), request.getBodyContent().size());
    }
    lseek(in_fd, 0, SEEK_SET);

    vector<string> env;
    env.push_back("REQUEST_METHOD=" + request.getMethod());
    env.push_back("SCRIPT_FILENAME=" + physicalPath);
    env.push_back("SCRIPT_NAME=" + request.getPath());
    
    size_t qPos = request.getPath().find('?');
    string query = "";
    if (qPos != string::npos)
    {
        query = request.getPath().substr(qPos + 1);
    }
    env.push_back("QUERY_STRING=" + query);
    env.push_back("SERVER_PROTOCOL=HTTP/1.1");
    env.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env.push_back("REDIRECT_STATUS=200");

    const map<string, string>& headers = request.getHeaders();
    map<string, string>::const_iterator it;
    if ((it = headers.find("content-length")) != headers.end())
        env.push_back("CONTENT_LENGTH=" + it->second);
    if ((it = headers.find("content-type")) != headers.end())
        env.push_back("CONTENT_TYPE=" + it->second);

    for (it = headers.begin(); it != headers.end(); ++it)
    {
        string key = it->first;
        for (size_t i = 0; i < key.size(); ++i)
        {
            if (key[i] == '-') key[i] = '_';
            else key[i] = toupper(key[i]);
        }
        env.push_back("HTTP_" + key + "=" + it->second);
    }

    vector<char*> envp;
    for (size_t i = 0; i < env.size(); ++i)
    {
        envp.push_back(const_cast<char*>(env[i].c_str()));
    }
    envp.push_back(NULL);

    pid_t pid = fork();
    if (pid == -1)
    {
        fclose(infile);
        fclose(outfile);
        throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
    }
    else if (pid == 0)
    {
        dup2(in_fd, STDIN_FILENO);
        dup2(out_fd, STDOUT_FILENO);

        fclose(infile);
        fclose(outfile);

        char *argv[3];
        if (!interpreter.empty())
        {
            argv[0] = const_cast<char*>(interpreter.c_str());
            argv[1] = const_cast<char*>(physicalPath.c_str());
            argv[2] = NULL;
        }
        else
        {
            argv[0] = const_cast<char*>(physicalPath.c_str());
            argv[1] = NULL;
        }

        execve(argv[0], argv, &envp[0]);
        exit(127);
    }

    int status;
    waitpid(pid, &status, 0);

    fclose(infile);

    if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
    {
        fclose(outfile);
        throw HttpException(STATUS_BAD_GATEWAY);
    }

    string cgi_output;
    char buf[4096];
    lseek(out_fd, 0, SEEK_SET);
    while (true)
    {
        ssize_t bytes_read = read(out_fd, buf, sizeof(buf));
        if (bytes_read <= 0)
            break;
        cgi_output.append(buf, bytes_read);
    }
    fclose(outfile);

    size_t separator = cgi_output.find("\r\n\r\n");
    size_t sep_len = 4;
    if (separator == string::npos)
    {
        separator = cgi_output.find("\n\n");
        sep_len = 2;
    }

    string body;
    int statusCode = 200;
    string statusMsg = "OK";
    map<string, string> cgiHeaders;

    if (separator != string::npos)
    {
        string headers_part = cgi_output.substr(0, separator);
        body = cgi_output.substr(separator + sep_len);

        stringstream ss(headers_part);
        string line;
        while (getline(ss, line))
        {
            if (!line.empty() && line[line.size() - 1] == '\r')
                line.erase(line.size() - 1);
            if (line.empty())
                continue;

            size_t colon = line.find(':');
            if (colon != string::npos)
            {
                string key = line.substr(0, colon);
                string val = line.substr(colon + 1);
                
                while (!key.empty() && (key[0] == ' ' || key[0] == '\t')) key.erase(0, 1);
                while (!key.empty() && (key[key.size() - 1] == ' ' || key[key.size() - 1] == '\t')) key.erase(key.size() - 1, 1);
                while (!val.empty() && (val[0] == ' ' || val[0] == '\t')) val.erase(0, 1);
                while (!val.empty() && (val[val.size() - 1] == ' ' || val[val.size() - 1] == '\t')) val.erase(val.size() - 1, 1);

                if (key == "Status")
                {
                    stringstream status_ss(val);
                    status_ss >> statusCode;
                    getline(status_ss, statusMsg);
                    while (!statusMsg.empty() && (statusMsg[0] == ' ' || statusMsg[0] == '\t'))
                        statusMsg.erase(0, 1);
                }
                else
                {
                    cgiHeaders[key] = val;
                }
            }
        }
    }
    else
    {
        body = cgi_output;
    }

    response.setStatusCode(statusCode);
    response.setMessage(statusMsg.empty() ? HttpResponse::getDefaultStatusMessage(statusCode) : statusMsg);
    
    for (map<string, string>::const_iterator it = cgiHeaders.begin(); it != cgiHeaders.end(); ++it)
    {
        response.setResponseHeader(it->first, it->second);
    }
    
    if (cgiHeaders.find("Content-Length") == cgiHeaders.end() && cgiHeaders.find("content-length") == cgiHeaders.end())
    {
        response.setResponseHeader("Content-Length", intToString(body.size()));
    }

    if (request.getMethod() == "HEAD")
    {
        response.setResponseBody("");
    }
    else
    {
        response.setResponseBody(body);
    }

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
