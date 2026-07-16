#include "HttpException.hpp"
#include "HttpErrors.hpp"

HttpException::HttpException(const std::string &msg) : message(msg)
{
    matchMessageWithStatusCode();
}
void HttpException::matchMessageWithStatusCode()
{
    if (message == STATUS_BAD_REQUEST ||
        message == ERR_INVALID_TARGET ||
        message == ERR_INVALID_PROTOCOL ||
        message == ERR_INVALID_CONTENT_LEN ||
        message == ERR_NO_REQUEST_LINE ||
        message == ERR_NO_METHOD ||
        message == ERR_NO_TARGET ||
        message == ERR_INVALID_HEADER_FMT ||
        message == ERR_NO_COLON ||
        message == ERR_EMPTY_KEY_VAL ||
        message == ERR_INVALID_HEX_SIZE ||
        message == ERR_INVALID_CHUNK_TERM)
    {
        statusCode = 400;
        statusMessage = STATUS_BAD_REQUEST;
    }
    else if (message == STATUS_UNAUTHORIZED)
    {
        statusCode = 401;
        statusMessage = STATUS_UNAUTHORIZED;
    }
    else if (message == STATUS_FORBIDDEN)
    {
        statusCode = 403;
        statusMessage = STATUS_FORBIDDEN;
    }
    else if (message == STATUS_NOT_FOUND)
    {
        statusCode = 404;
        statusMessage = STATUS_NOT_FOUND;
    }
    else if (message == STATUS_METHOD_NOT_ALLOWED || message == ERR_UNSUPPORTED_METHOD)
    {
        statusCode = 405;
        statusMessage = STATUS_METHOD_NOT_ALLOWED;
    }
    else if (message == STATUS_PAYLOAD_TOO_LARGE)
    {
        statusCode = 413;
        statusMessage = STATUS_PAYLOAD_TOO_LARGE;
    }
    else if (message == STATUS_INTERNAL_SERVER_ERROR || message == ERR_READ)
    {
        statusCode = 500;
        statusMessage = STATUS_INTERNAL_SERVER_ERROR;
    }
    else if (message == STATUS_NOT_IMPLEMENTED)
    {
        statusCode = 501;
        statusMessage = STATUS_NOT_IMPLEMENTED;
    }
    else if (message == STATUS_BAD_GATEWAY)
    {
        statusCode = 502;
        statusMessage = STATUS_BAD_GATEWAY;
    }
    else if (message == NOT_EXTENDED)
    {
        statusCode = 510;
        statusMessage = NOT_EXTENDED;
    }
    else if (message == STATUS_HTTP_VERSION_NOT_SUPPORTED || message == ERR_UNSUPPORTED_VERSION)
    {
        statusCode = 505;
        statusMessage = STATUS_HTTP_VERSION_NOT_SUPPORTED;
    }
    else
        statusCode = 0;
}

const std::string &HttpException::getStatusMessage() const
{
    return statusMessage;
}

int HttpException::getStatusCode() const
{
    return statusCode;
}

const char *HttpException::what() const throw()
{
    return message.c_str();
}