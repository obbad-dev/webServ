#include <string>
#include <vector>
#include <sstream>

static std::vector<std::string> split(const std::string& path)
{
    std::vector<std::string> parts;
    std::stringstream stream(path);
    std::string part;

    while (std::getline(stream, part, '/'))
    {
        if (!part.empty())
            parts.push_back(part);
    }
    return parts;
}

static bool resolveParts(const std::vector<std::string>& input, std::vector<std::string>& output, bool rejectEscape = false, size_t minDepth = 0)
{
	if (input.empty())
        return true;
    for (size_t i = 0; i < input.size(); ++i)
    {
        const std::string& part = input[i];

        if (part == "." || part.empty())
            continue;

        if (part == "..")
        {
            if (output.size() <= minDepth)
            {
                if (rejectEscape)
                    continue;
                return false;
            }

            output.pop_back();
        }
        else
            output.push_back(part);
    }
    return true;
}

bool realPath(const std::string& root, const std::string& uri, std::string& result)
{
    std::vector<std::string> rootParts;
    std::vector<std::string> uriParts;
    std::vector<std::string> path;

    if (root.empty())
        return false;

    rootParts = split(root);
    uriParts = split(uri);

    if (!resolveParts(rootParts, path, true))
        return false;

    const size_t rootDepth = path.size();

    if (!resolveParts(uriParts, path, false, rootDepth))
        return false;

    if (root[0] == '/')
        result = "/";
    for (size_t i = 0; i < path.size(); ++i)
    {
        if (i != 0)
            result.append("/");
        result.append(path[i]);
    }
    return true;
}













