#include <string>
#include <sstream>

std::string intToString(int number)
{
    std::stringstream ss;
    ss << number;
    return ss.str();
}