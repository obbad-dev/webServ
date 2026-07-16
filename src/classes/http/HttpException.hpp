#include <exception>
#include <string>

class HttpException : public std::exception
{
private:
    int statusCode;
    std::string message;
    std::string statusMessage;
    void matchMessageWithStatusCode();

public:
    HttpException(const std::string &msg);
    int getStatusCode() const;
    const std::string &getStatusMessage() const;
    virtual const char *what() const throw();
};