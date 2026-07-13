#include "HttpResponse.hpp"

// bool read_content(string &content, string &path)
// {
//     ifstream file(path.c_str());
//     if (!file.is_open())
//         return false;

//     string tmp_content;
//     while (1)
//     {
//         getline(file, tmp_content);
//         content += tmp_content;
//         if (file.eof())
//             break;
//         content += "\n";
//     }
//     return true;
// }

// string conv_to_str(int number)
// {
//     ostringstream s;
//     s << number;
//     return s.str();
// }

// string retrieve_extension(map<string, string> &extensions, string &path)
// {
//     string substring;
//     size_t pos = path.rfind(".");
//     if (pos != string::npos)
//         substring = path.substr(pos);
//     else
//         return "Not Extended";

//     map<string, string>::iterator it = extensions.find(substring);
//     if (it != extensions.end())
//         return it->second;
//     else
//         return "Not Extended";
// }

// void HttpResponse::create_response(int clientFd, map<string, string> &extensions)
// {
//     string response;
//     if (method == "GET")
//     {
//         string path;
//         if (target == "/")
//             path = "./resources/sites/index.html";
//         else
//             path = "./resources/sites" + target;

//         string content;
//         if (read_content(content, path))
//         {
//             response += "HTTP/1.1 200 OK\r\n";
//             response += "Content-Type: ";
//             response += retrieve_extension(extensions, path);
//             response += "\r\n";
//             response += "Content-Length: ";
//             response += conv_to_str(content.size());
//             response += "\r\n";
//             response += "\r\n";
//             response += content;
//         }
//         else
//         {
//             response += "HTTP/1.1 404 Not Found\r\n";
//             response += "Content-Type: text/plain\r\n";
//             response += "Content-Length: 9\r\n";
//             response += "\r\n";
//             response += "Not Found";
//         }
//     }
    // else if (method == "POST")
    // {

    // }
    // else if (method = "DELETE")
    // {

    // }
    // else
    // {
    //     // send ERROR;
    // }
// }