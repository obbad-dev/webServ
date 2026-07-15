#include <string>
#include <vector>
#include <sstream>

// Splits a path into its non-empty components.
// "/a//b/./c" -> ["a", "b", ".", "c"]
static std::vector<std::string> split(const std::string &path)
{
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string part;

    while (std::getline(ss, part, '/'))
        if (!part.empty())
            parts.push_back(part);
    return parts;
}

// Resolves `uri` against `root` and writes the absolute result into
// `result`. Returns false if the resolved path would escape `root`.
//
// Purely lexical: normalizes "." and ".." components but does NOT
// touch the filesystem or follow symlinks. `root` must be given as
// an absolute path (starting with '/').
//
// NOTE: `uri` must already be percent-decoded before being passed in,
// otherwise encoded traversal sequences (e.g. "%2e%2e%2f") will slip
// through unnoticed.
bool realPath(const std::string &root, const std::string &uri, std::string &result)
{
    if (root.empty() || root[0] != '/')
        return false;

    std::vector<std::string> parts = split(root);
    size_t rootDepth = parts.size();

    std::vector<std::string> uriParts = split(uri);
    for (size_t i = 0; i < uriParts.size(); ++i)
        parts.push_back(uriParts[i]);

    std::vector<std::string> stack;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        const std::string &part = parts[i];

        if (part == ".")
            continue;
        if (part == "..")
        {
            if (stack.size() <= rootDepth)
                return false; // would climb above the document root
            stack.pop_back();
        }
        else
            stack.push_back(part);
    }

    result = "/";
    for (size_t i = 0; i < stack.size(); ++i)
    {
        result += stack[i];
        if (i + 1 != stack.size())
            result += "/";
    }
    return true;
}