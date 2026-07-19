# HttpResponseBuilder — Deep Dive

> This document teaches you how `HttpResponseBuilder` works from the top level down to
> individual lines. After reading it you should be able to confidently debug, extend and
> modify the class on your own.

---

## 1. Where does `HttpResponseBuilder` fit in the server?

Before looking at a single line, understand the object that is passed into every function:
`FdManager`. It is a **struct** that bundles together everything known about one open
connection:

```
FdManager
├── HttpRequest  request      ← the parsed incoming request
├── HttpResponse response     ← the response being built (starts empty)
└── Server       blockServer  ← the virtual-host config that owns this connection
```

The server's event loop parses the HTTP request into `request`, then calls
`HttpResponseBuilder::build(manager)`. The builder's **only job** is to fill `response`
and call `response.serializeResponse()` so the raw bytes are ready to send.

---

## 2. Overall Flow — Annotated Flowchart

```
                        HttpResponseBuilder::build(manager)
                                       │
                    ┌──────────────────▼──────────────────────┐
                    │  1. Body size check                       │
                    │  request.body.size > client_max_body?     │
                    └──────────┬──────────────────┬────────────┘
                               │ NO               │ YES
                               ▼                  ▼
                    ┌──────────────────┐   throw 413 Payload Too Large
                    │ 2. findLocation()│
                    │ Longest Prefix   │
                    │ Match on URI     │
                    └────────┬─────────┘
                             │
              ┌──────────────▼──────────────────┐
              │ 3. location has "return" rule?   │
              └──────┬─────────────────┬─────────┘
                     │ YES             │ NO
                     ▼                 ▼
              301/302 redirect   ┌─────────────────────────┐
              → return           │ 4. Method allowed?       │
                                 │ location.allowMethods    │
                                 └──────┬──────────┬────────┘
                                        │ YES      │ NO
                                        ▼          ▼
                                   continue   405 + Allow header → return
                                        │
                          ┌─────────────▼─────────────────┐
                          │ 5. Build physical path         │
                          │ root + request.path            │
                          │ realPath() → path traversal    │
                          │ check (403 if escape)          │
                          └─────────────┬─────────────────┘
                                        │
                          ┌─────────────▼─────────────────┐
                          │ 6. stat() the path             │
                          │ Does it exist?                 │
                          └──────┬────────────┬────────────┘
                                 │ NO         │ YES
                                 ▼            ▼
                              404 Not   ┌─────────────────────────────┐
                              Found     │ Is it a DIRECTORY?           │
                                        └──────┬──────────┬────────────┘
                                               │ YES      │ NO → (file)
                                               ▼          │
                                  ┌────────────────────┐  │
                                  │ Try index files    │  │
                                  │ e.g. index.html    │  │
                                  └──────┬─────────────┘  │
                                         │ found          │
                                         ▼                │
                                  physicalPath = index ──▶│
                                         │ not found      │
                                         ▼                │
                                  ┌──────────────────┐    │
                                  │ autoindex ON?    │    │
                                  └──┬────────────┬──┘    │
                                     │ YES        │ NO    │
                                     ▼            ▼       │
                              generateDirectory  403    (falls through
                              Listing() → 200 Forbidden   to file path)
                              HTML → return               │
                                                          ▼
                                          ┌───────────────────────────┐
                                          │ 7. CGI extension check    │
                                          │ location.cgiPass[.ext]?   │
                                          └──────┬──────────┬─────────┘
                                                 │ YES      │ NO
                                                 ▼          ▼
                                          executeCGI()   Method dispatch
                                          → return       ┌──────────────┐
                                                         │ GET          │
                                                         │ readBinary   │
                                                         │ File() → 200 │
                                                         ├──────────────┤
                                                         │ POST         │
                                                         │ upload to    │
                                                         │ uploadPath   │
                                                         │ → 201        │
                                                         ├──────────────┤
                                                         │ DELETE       │
                                                         │ remove()     │
                                                         │ → 204        │
                                                         └──────────────┘
                                                                │
                                                                ▼
                                                  response.serializeResponse()
```

**The key architectural insight:** everything in `build()` that detects a problem throws an
`HttpException`. A single `try/catch` block at the bottom catches **all** errors and
converts them into a proper error response. This means error handling is centralised —
you never have to write `if (error) buildErrorResponse()` twice.

---

## 3. Function-by-Function Reference

---

### 3.1 `build()` — The Dispatcher

**Purpose:** The only `public` function. It is the entry point that orchestrates every
step of response construction in the correct order.

**Called when:** The server's event loop has finished parsing an HTTP request and is ready
to generate a response.

**Inputs:** `FdManager& manager` — contains the request, the (empty) response, and the
server config.

**Output:** None (void). It mutates `manager.response` in-place.

**Interactions with other functions:**
- Calls `findLocation()` to get the config block.
- Calls `generateDirectoryListing()` for autoindex.
- Calls `readBinaryFile()` for static GET.
- Calls `executeCGI()` for dynamic scripts.
- Calls `getMimeType()` and `intToString()` as helpers.

**Why it exists:** Separation of concerns. The `build()` function is the *controller*. It
decides *what* to do; the private helpers decide *how* to do it.

---

### 3.2 `findLocation()` — Longest Prefix Match

**Purpose:** Given the request URI (e.g. `/images/logo.png`) and the server config,
return the **most specific** `LocationConf` block that matches.

**Called when:** Early in `build()`, before any path or file operations.

**Inputs:**
- `const string& requestPath` — the URI path from the request line (e.g. `/images/logo.png`)
- `const Server& server` — the virtual host config containing all location blocks

**Output:** `const LocationConf*` — pointer to the best-matching location, or `NULL` if
no location matches.

**Why "longest prefix"?** Nginx-style configuration matches the most *specific* rule. If
you have `/` and `/images/`, a request for `/images/logo.png` should match `/images/`
because it is more specific. The function tracks `maxLen` (the length of the best match
found so far) and only updates `bestMatch` when a longer prefix is found.

---

### 3.3 `generateDirectoryListing()` — Autoindex Page Builder

**Purpose:** When a request targets a directory that has no index file but has `autoindex
on`, produce an HTML page listing all the directory's contents.

**Called when:** `build()` determines the path is a directory, no index file was found,
and `location->hasAutoindex()` is true.

**Inputs:**
- `const string& dirPath` — the filesystem path to the directory (e.g. `/var/www/files/`)
- `const string& uriPath` — the URI path shown in the browser (e.g. `/files/`)

**Output:** `string` — a complete HTML document, or an empty string on failure.

**How it interacts:** Its return value is placed directly into `response.setResponseBody()`.

---

### 3.4 `readBinaryFile()` — Safe File Reader

**Purpose:** Read the entire content of any file (text or binary) into a `std::string`.

**Called when:** A GET request targets a static file that is not a CGI script.

**Inputs:**
- `const string& filepath` — absolute path to the file
- `string& content` — output parameter; the file's bytes are written here

**Output:** `bool` — `true` on success, `false` if the file could not be opened or read.

**Why it exists:** Centralises all file reading so `build()` stays clean. Using
`ios::binary` ensures no newline translation occurs, which is critical for images, PDFs,
and other binary files.

---

### 3.5 `executeCGI()` — Dynamic Script Executor

**Purpose:** Run an external script (Python, PHP, etc.), pass it the HTTP request data
via environment variables and stdin, capture its stdout, and parse it into a proper HTTP
response.

**Called when:** The resolved file's extension matches an entry in `location->getCgiPass()`
(e.g. `.py → /usr/bin/python3`).

**Inputs:**
- `FdManager& manager` — the full connection context (request + response)
- `const string& physicalPath` — filesystem path to the script
- `const string& interpreter` — path to the interpreter binary (e.g. `/usr/bin/python3`)

**Output:** None (void). Writes directly into `manager.response` and calls
`serializeResponse()` itself.

**Why it calls `serializeResponse()` itself:** CGI is the one case that fully
short-circuits `build()`'s final `serializeResponse()` call (via `return` after
`executeCGI()`). It therefore must serialize its own response before returning.

---

### 3.6 `getMimeType()` — Content-Type Resolver

**Purpose:** Given a file path, return the correct MIME type string (e.g. `text/html`,
`image/png`).

**Inputs:**
- `const string& path` — file path (used only to extract the extension)
- `const string& defaultMime` — fallback MIME type if the extension is unknown

**Output:** `string` — the MIME type.

**How it works:** Extracts the extension from the path using `rfind('.')`, then looks it
up in `FdManager::extensions` — a static `map<string,string>` populated at startup from
the server configuration.

---

### 3.7 `intToString()` — Number-to-String Utility

**Purpose:** Convert an integer to a `std::string`.

**Why it exists:** The project targets C++98, where `std::to_string()` does not exist
(it was introduced in C++11). Using a `stringstream` is the idiomatic C++98 solution.

---

## 4. Line-by-Line Explanation

---

### 4.1 Includes and Setup (lines 1–14)

```cpp
#include "HttpResponseBuilder.hpp"   // This class's own header
#include "ServerSide.hpp"            // Defines FdManager, which holds request+response+server
#include "helperFunc.hpp"            // Contains realPath() — the path-traversal guard
#include <sstream>                   // std::stringstream — used for number conversion and HTML building
#include <iostream>                  // std::cerr — (mostly unused in this file directly)
#include <fstream>                   // std::ifstream/ofstream — for reading/writing files
#include <unistd.h>                  // POSIX: write(), read(), lseek(), close(), dup2(), fork(), execve()
#include <sys/stat.h>                // stat() — get filesystem metadata (size, type, permissions)
#include <sys/types.h>               // pid_t, size_t, ssize_t
#include <sys/wait.h>                // waitpid(), WIFEXITED(), WEXITSTATUS()
#include <dirent.h>                  // opendir(), readdir(), closedir() — directory iteration
#include <cstdlib>                   // std::remove() (delete a file), exit()
#include <cstdio>                    // NULL
#include <fcntl.h>                   // open(), O_CREAT, O_RDWR, O_TRUNC — CGI temp files
```

---

### 4.2 `build()` — Line by Line

```cpp
void HttpResponseBuilder::build(FdManager &manager)
```
Static method — no `this` pointer. Works only on the `manager` argument.

```cpp
HttpRequest  &request  = manager.request;
HttpResponse &response = manager.response;
const Server &server   = manager.blockServer;
```
**Aliases.** These references avoid writing `manager.request` on every line. Because they
are references (not copies), every write to `response` modifies the real object inside
`manager`.

```cpp
try {
```
Everything inside can throw `HttpException`. If anything fails, the single `catch` block
at the end handles it. This is the **gateway pattern** for error handling.

---

#### Step 1 — Body Size Check (lines 25–28)
```cpp
if (server.hasSetClientMaxBodySize() &&
    request.getBodyContent().size() > server.getClientMaxBodySize())
{
    throw HttpException(STATUS_PAYLOAD_TOO_LARGE);
}
```
- `hasSetClientMaxBodySize()` — returns `true` only if the config file has a
  `client_max_body_size` directive. Without it, the check is skipped entirely.
- `getBodyContent().size()` — returns the number of bytes in the request body.
- `STATUS_PAYLOAD_TOO_LARGE` — the string `"Payload Too Large"` (HTTP 413). Defined in
  `HttpErrors.hpp`.
- **HTTP concept:** 413 is the correct status code for a body that exceeds the server's
  configured limit.

---

#### Step 2 — Location Lookup (line 31)
```cpp
const LocationConf *location = findLocation(request.getPath(), server);
```
- `request.getPath()` returns the URI path portion (e.g. `/images/photo.jpg`).
- The result can be `NULL` — meaning no `location` block matched. The code handles this
  gracefully throughout by checking `if (location)` before accessing its fields.

---

#### Step 3 — Redirection (lines 34–43)
```cpp
if (location && location->hasReturn())
{
    pair<int, string> redir = location->getReturn();
    response.setStatusCode(redir.first);           // e.g. 301 or 302
    response.setMessage(HttpResponse::getDefaultStatusMessage(redir.first));
    response.setResponseHeader("Location", redir.second); // the target URL
    response.setResponseBody("Redirecting...");
    response.serializeResponse(...);
    return;  // ← short-circuit: nothing else to do
}
```
- `hasReturn()` — true if the location block has a `return` directive (e.g.
  `return 301 https://new-site.com`).
- `getReturn()` — returns a `pair<int,string>`: status code + target URL.
- The `Location` header is what browsers read to follow a redirect.
- **HTTP concept:** 301 = permanent redirect, 302 = temporary redirect.

---

#### Step 4 — Method Verification (lines 46–64)
```cpp
if (location)
{
    const set<string> &allowed = location->getAllowMethods();
    if (allowed.find(request.getMethod()) == allowed.end())
    {
        response.buildErrorResponse(HttpException(STATUS_METHOD_NOT_ALLOWED), server);
        // Build the "Allow: GET, POST" header listing all accepted methods
        string allowHeader;
        for (set<string>::const_iterator it = allowed.begin(); it != allowed.end(); ++it)
        {
            if (it != allowed.begin())
                allowHeader += ", ";
            allowHeader += *it;
        }
        response.setResponseHeader("Allow", allowHeader);
        response.serializeResponse(...);
        return;
    }
}
```
- If no location matched (`location == NULL`), this block is skipped — any method is
  implicitly allowed.
- `allowed.find(...) == allowed.end()` — the classic STL idiom for "element not found in
  a set".
- **HTTP concept:** RFC 7231 requires that a 405 response **must** include an `Allow`
  header listing the permitted methods. The loop builds that header.
- **C++ concept:** `std::set` iterators are sorted, so the Allow header will always be in
  alphabetical order.

---

#### Step 5 — Physical Path Construction (lines 67–72)
```cpp
string root = (location && location->rootIsSet()) ? location->getRoot() : server.getRoot();
string physicalPath;
if (!realPath(root, request.getPath(), physicalPath))
{
    throw HttpException(STATUS_FORBIDDEN);
}
```
- The ternary picks the most specific root: a location-level `root` overrides the
  server-level `root`.
- `realPath()` (from `helperFunc.hpp`) combines `root + path` and **detects path
  traversal attacks**. If the resulting path escapes the root directory (e.g. someone
  requested `/../../../etc/passwd`), it returns `false` and we throw 403.
- **Security concept:** Without this check, an attacker could read arbitrary files on the
  system.

---

#### Step 6 — Directory vs File Resolution (lines 75–135)

```cpp
struct stat pathStat;
if (stat(physicalPath.c_str(), &pathStat) != 0)
{
    throw HttpException(STATUS_NOT_FOUND);
}
```
- `stat()` fills a `struct stat` with metadata about a file/directory. Returns `0` on
  success, `-1` on failure (file does not exist → 404).
- `pathStat.st_mode` encodes the file type and permissions.

```cpp
if (S_ISDIR(pathStat.st_mode))
```
- `S_ISDIR` is a POSIX macro that checks the mode bits for "is this a directory?".

**Index file search (lines 86–100):**
```cpp
for (size_t i = 0; i < indexes.size(); ++i)
{
    string testIndex = physicalPath;
    if (testIndex[testIndex.size() - 1] != '/')
        testIndex += "/";
    testIndex += indexes[i];           // e.g. /var/www/html/index.html

    struct stat indexStat;
    if (stat(testIndex.c_str(), &indexStat) == 0 && S_ISREG(indexStat.st_mode))
    {
        physicalPath = testIndex;      // found! re-point physicalPath to the index file
        indexFound = true;
        break;
    }
}
```
- Tries each configured index file name in order until one is found.
- `S_ISREG` — checks that the found path is a regular file (not another directory, not a
  symlink, etc.).

**Autoindex fallback (lines 102–124):**
```cpp
if (!indexFound)
{
    if (location && location->hasAutoindex())
    {
        string listing = generateDirectoryListing(physicalPath, request.getPath());
        if (listing.empty())
            throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
        response.setStatusCode(200);
        response.setMessage("OK");
        response.setResponseHeader("Content-Type", "text/html");
        response.setResponseHeader("Content-Length", intToString(listing.size()));
        response.setResponseBody(listing);
        response.serializeResponse(...);
        return;   // ← done, it was a directory listing
    }
    else
        throw HttpException(STATUS_FORBIDDEN);
}
```
- If no index file was found and autoindex is off, a directory listing would expose
  server structure — so 403 Forbidden is returned.

**Second stat after index resolution (lines 127–136):**
```cpp
if (stat(physicalPath.c_str(), &pathStat) != 0)
    throw HttpException(STATUS_NOT_FOUND);
if (!S_ISREG(pathStat.st_mode))
    throw HttpException(STATUS_FORBIDDEN);
```
- After potentially replacing `physicalPath` with an index file, we re-stat the path to
  refresh `pathStat` with the new file's metadata.
- The `!S_ISREG` check ensures we never try to read a device file, socket, or FIFO as if
  it were a regular file.

---

#### Step 7 — CGI Detection (lines 140–156)
```cpp
string ext = "";
size_t dotPos = physicalPath.rfind('.');
if (dotPos != string::npos)
    ext = physicalPath.substr(dotPos);  // e.g. ".py", ".php"

if (location)
{
    const map<string, string> &cgiMap = location->getCgiPass();
    map<string, string>::const_iterator cgiIt = cgiMap.find(ext);
    if (cgiIt != cgiMap.end())
    {
        executeCGI(manager, physicalPath, cgiIt->second);
        return;  // ← CGI handles serialization itself
    }
}
```
- `rfind('.')` finds the **last** dot, so a file like `archive.tar.gz` correctly yields
  `.gz`.
- `cgiMap` maps extension → interpreter path: e.g. `{".py": "/usr/bin/python3"}`.
- If the extension is in the map, hand off to `executeCGI()` entirely.

---

#### Step 8 — Method Dispatch (lines 158–229)

**GET:**
```cpp
if (request.getMethod() == "GET")
{
    string body;
    if (!readBinaryFile(physicalPath, body))
        throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
    response.setStatusCode(200);
    response.setMessage("OK");
    response.setResponseHeader("Content-Type", getMimeType(physicalPath, "application/octet-stream"));
    response.setResponseHeader("Content-Length", intToString(body.size()));
    response.setResponseBody(body);
}
```
- `getMimeType()` is called with `"application/octet-stream"` as the fallback — this is
  the RFC-recommended default for "unknown binary data".
- **HTTP concept:** `Content-Length` must be the exact byte count of the body so the
  client knows when the response ends.

**POST — File Upload:**
```cpp
else if (request.getMethod() == "POST")
{
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
            filename = "upload_" + intToString(time(NULL));

        string finalUploadPath = uploadDir;
        if (finalUploadPath[finalUploadPath.size() - 1] != '/')
            finalUploadPath += "/";
        finalUploadPath += filename;

        ofstream outFile(finalUploadPath.c_str(), ios::out | ios::binary);
        if (!outFile.is_open())
            throw HttpException(STATUS_INTERNAL_SERVER_ERROR);
        const string &fileData = request.getBodyContent();
        outFile.write(fileData.data(), fileData.size());
        outFile.close();

        response.setStatusCode(201);
        response.setMessage("Created");
        ...
    }
    else
        throw HttpException(STATUS_METHOD_NOT_ALLOWED);
}
```
- The filename is derived from the URI path's last segment. If none is usable,
  `time(NULL)` (seconds since Unix epoch) makes it unique.
- `ios::out | ios::binary` — always write binary to avoid platform-specific newline
  translation corrupting the file.
- **HTTP concept:** 201 Created is the correct status for a successful resource creation.

**DELETE:**
```cpp
else if (request.getMethod() == "DELETE")
{
    if (std::remove(physicalPath.c_str()) == 0)
    {
        response.setStatusCode(204);
        response.setMessage("No Content");
        response.setResponseBody("");
    }
    else
        throw HttpException(STATUS_FORBIDDEN);
}
```
- `std::remove()` deletes a file. Returns `0` on success, non-zero on failure.
- **HTTP concept:** 204 No Content means "success, but I have no body to send." This is
  the standard response for DELETE.

---

#### The catch blocks (lines 233–242)
```cpp
catch (const HttpException &e)
{
    response.buildErrorResponse(e, server);
    response.serializeResponse(...);
}
catch (const std::exception &e)
{
    response.buildErrorResponse(HttpException(STATUS_INTERNAL_SERVER_ERROR), server);
    response.serializeResponse(...);
}
```
- Two catches: one for HTTP-specific errors (known status codes), one for any unexpected
  C++ exception (500 Internal Server Error).
- `buildErrorResponse()` consults the server config for a custom error page, or
  generates a default HTML page if none is configured.

---

### 4.3 `findLocation()` — Line by Line

```cpp
const LocationConf* HttpResponseBuilder::findLocation(const string& requestPath, const Server& server)
{
    const LocationConf* bestMatch = NULL;   // Start with no match
    size_t maxLen = 0;                       // Length of the best matching prefix found so far

    // const_cast needed because getLocations() is not const on Server — a design smell
    const vector<LocationConf>& locations = const_cast<Server&>(server).getLocations();

    for (size_t i = 0; i < locations.size(); ++i)
    {
        const string& locPath = locations[i].getPath();

        // Does the request path START WITH this location's path?
        if (requestPath.find(locPath) == 0)
        {
            // Is this a longer (more specific) match than what we already found?
            if (locPath.length() > maxLen)
            {
                maxLen = locPath.length();
                bestMatch = &locations[i];  // remember the address of this LocationConf
            }
        }
    }
    return bestMatch;
}
```

**Example:** Config has `/` and `/api/`. Request path is `/api/users`.
- `/` matches at position 0, length 1.
- `/api/` matches at position 0, length 5 → **winner**.
- Returns pointer to the `/api/` LocationConf.

---

### 4.4 `generateDirectoryListing()` — Line by Line

```cpp
DIR *dir = opendir(dirPath.c_str());
if (!dir)
    return "";   // Can't open → caller will throw 500
```
`opendir()` is the POSIX call to open a directory stream. Returns `NULL` on failure (e.g.
no read permission).

```cpp
stringstream ss;
ss << "<html><head><title>Index of " << uriPath << "</title></head><body>\n";
ss << "<h1>Index of " << uriPath << "</h1><hr><ul>\n";
```
Building the HTML incrementally into a `stringstream`. The `uriPath` is the browser-facing
path (not the filesystem path), which is what you want to display.

```cpp
if (uriPath != "/" && !uriPath.empty())
    ss << "<li><a href=\"../\">../ (Parent Directory)</a></li>\n";
```
Adds a "go up" link unless we are at the root — there is no parent above `/`.

```cpp
struct dirent *entry;
while ((entry = readdir(dir)) != NULL)
{
    string name = entry->d_name;
    if (name == "." || name == "..")
        continue;
```
`readdir()` returns one directory entry per call, `NULL` when done. We skip `.` (current)
and `..` (parent) because we already handled the parent link above.

```cpp
    string fullPath = dirPath;
    if (fullPath[fullPath.size() - 1] != '/')
        fullPath += "/";
    fullPath += name;

    struct stat s;
    bool isDir = false;
    if (stat(fullPath.c_str(), &s) == 0)
    {
        if (S_ISDIR(s.st_mode))
            isDir = true;
    }
```
We `stat()` each entry to find out if it is itself a directory, so we can append a `/`
to its link.

```cpp
    string linkName = name;
    if (isDir)
        linkName += "/";

    ss << "<li><a href=\"" << linkName << "\">" << linkName << "</a></li>\n";
}
closedir(dir);
ss << "</ul><hr></body></html>";
return ss.str();
```
`closedir()` releases the directory handle — always necessary to avoid resource leaks.
`ss.str()` extracts the accumulated string.

---

### 4.5 `readBinaryFile()` — Line by Line

```cpp
ifstream file(filepath.c_str(), ios::in | ios::binary | ios::ate);
```
- `ios::in` — open for reading.
- `ios::binary` — do not translate `\n` to `\r\n` on Windows (important for correctness
  even on Linux to ensure no transformation occurs).
- `ios::ate` — **A**t **T**h**e** **E**nd: the read cursor starts at the end of the file.
  This is the trick used to measure file size without a separate `seekg`.

```cpp
if (!file.is_open())
    return false;

streamsize size = file.tellg();   // Position = end of file = file size in bytes
file.seekg(0, ios::beg);          // Rewind to the beginning
```
`tellg()` returns the current cursor position as a `streamsize` (signed). At `ios::ate`,
this equals the file size.

```cpp
content.resize(size);             // Pre-allocate exactly the right number of bytes
if (file.read(&content[0], size)) // Read everything in one shot
    return true;
return false;
```
`resize()` + single `read()` is more efficient than reading chunk by chunk.
`&content[0]` gives a raw pointer to the string's internal buffer (valid in C++98).

---

### 4.6 `executeCGI()` — Line by Line

This is the most complex function. Take it in stages.

**Stage 1 — Create temporary files:**
```cpp
static int cgi_counter = 0;
string in_name  = "/tmp/cgi_in_"  + intToString(cgi_counter);
string out_name = "/tmp/cgi_out_" + intToString(cgi_counter++);
```
- `static` local variable: initialised once, persists across calls. `cgi_counter++`
  evaluates to the old value (used for naming) then increments.
- Two temp files: one for stdin data going *into* the script, one for stdout coming *out*.

```cpp
int in_fd  = open(in_name.c_str(),  O_CREAT | O_RDWR | O_TRUNC, 0666);
int out_fd = open(out_name.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666);
```
- `O_CREAT` — create if not exists.
- `O_RDWR` — open for both reading and writing (we write then rewind and read).
- `O_TRUNC` — if the file exists from a previous run, truncate it to zero length.
- `0666` — file permissions: owner/group/other can read and write.

**Stage 2 — Write request body to stdin file:**
```cpp
if (!request.getBodyContent().empty())
    write(in_fd, request.getBodyContent().data(), request.getBodyContent().size());
lseek(in_fd, 0, SEEK_SET);
```
After writing, `lseek()` rewinds the file cursor back to the start so the child process
can read from the beginning.

**Stage 3 — Build the CGI environment:**
```cpp
vector<string> env;
env.push_back("REQUEST_METHOD=" + request.getMethod());
env.push_back("SCRIPT_FILENAME=" + physicalPath);
env.push_back("SCRIPT_NAME=" + request.getPath());
```
These are **mandatory CGI/1.1 environment variables** (RFC 3875). Without them, CGI
scripts like PHP cannot function.

```cpp
size_t qPos = request.getPath().find('?');
string query = "";
if (qPos != string::npos)
    query = request.getPath().substr(qPos + 1);
env.push_back("QUERY_STRING=" + query);
```
Extracts everything after `?` in the URL. E.g. `/search?q=hello` → `QUERY_STRING=q=hello`.

```cpp
env.push_back("SERVER_PROTOCOL=HTTP/1.1");
env.push_back("GATEWAY_INTERFACE=CGI/1.1");
env.push_back("REDIRECT_STATUS=200");
```
`REDIRECT_STATUS=200` is specifically required by PHP-CGI to allow it to run at all.
Without it, PHP refuses to execute.

```cpp
if ((it = headers.find("content-length")) != headers.end())
    env.push_back("CONTENT_LENGTH=" + it->second);
if ((it = headers.find("content-type")) != headers.end())
    env.push_back("CONTENT_TYPE=" + it->second);
```
These two headers get special CGI variable names (without the `HTTP_` prefix) per the
CGI spec.

```cpp
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
```
All other headers become `HTTP_HEADER_NAME=value` (hyphens converted to underscores,
uppercased). E.g. `Accept-Language: fr` → `HTTP_ACCEPT_LANGUAGE=fr`.

```cpp
vector<char*> envp;
for (size_t i = 0; i < env.size(); ++i)
    envp.push_back(const_cast<char*>(env[i].c_str()));
envp.push_back(NULL);
```
`execve()` requires a `char**` (null-terminated array of C-strings). This converts the
`vector<string>` into that format. `const_cast` is needed because `c_str()` returns
`const char*` but `execve` takes `char*`.

> ⚠️ **Important:** `envp` holds raw pointers into the `env` strings. If `env` goes out
> of scope or is modified, those pointers become dangling. Here it is safe because `env`
> and `envp` are in the same scope.

**Stage 4 — Fork:**
```cpp
pid_t pid = fork();
if (pid == -1) { /* cleanup + throw 500 */ }
else if (pid == 0)
{
    // CHILD PROCESS
    dup2(in_fd,  STDIN_FILENO);   // stdin  → reads from the input file
    dup2(out_fd, STDOUT_FILENO);  // stdout → writes to the output file

    close(in_fd);   // close the originals — dup2 already installed them as 0 and 1
    close(out_fd);

    char *argv[3];
    if (!interpreter.empty())
    {
        argv[0] = const_cast<char*>(interpreter.c_str());  // e.g. "/usr/bin/python3"
        argv[1] = const_cast<char*>(physicalPath.c_str()); // e.g. "/var/www/script.py"
        argv[2] = NULL;
    }
    else
    {
        argv[0] = const_cast<char*>(physicalPath.c_str());
        argv[1] = NULL;
    }

    execve(argv[0], argv, &envp[0]);
    exit(127);  // execve only returns on failure; 127 = "command not found"
}
```
- `fork()` creates an exact copy of the process. The parent gets the child's PID; the
  child gets 0.
- `dup2(src, dst)` — duplicates `src` file descriptor onto `dst`. After
  `dup2(in_fd, STDIN_FILENO)`, file descriptor 0 (stdin) now points to the temp input
  file.
- `execve()` **replaces** the child process image with the interpreter. The environment
  we built is passed as the third argument.

**Stage 5 — Parent waits for child:**
```cpp
int status;
waitpid(pid, &status, 0);   // Block until child exits
close(in_fd);               // Parent no longer needs these
```
`waitpid()` prevents zombie processes. Without it, the child would remain as a zombie in
the process table.

```cpp
if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
{
    close(out_fd);
    std::remove(in_name.c_str());
    std::remove(out_name.c_str());
    throw HttpException(STATUS_BAD_GATEWAY);
}
```
- `WIFEXITED` — true if the child exited normally (not killed by a signal).
- `WEXITSTATUS` — extracts the exit code. A non-zero code means the script failed.
- **HTTP concept:** 502 Bad Gateway is the correct status when an upstream process
  (CGI script) fails.

**Stage 6 — Read the script's output:**
```cpp
lseek(out_fd, 0, SEEK_SET);   // Rewind to beginning
while (true)
{
    ssize_t bytes_read = read(out_fd, buf, sizeof(buf));
    if (bytes_read <= 0)
        break;
    cgi_output.append(buf, bytes_read);
}
close(out_fd);
std::remove(in_name.c_str());
std::remove(out_name.c_str());
```
Read the output file 4096 bytes at a time into `cgi_output`. Cleanup temp files when done.

**Stage 7 — Parse CGI output headers:**
```cpp
size_t separator = cgi_output.find("\r\n\r\n");
size_t sep_len = 4;
if (separator == string::npos)
{
    separator = cgi_output.find("\n\n");
    sep_len = 2;
}
```
A CGI script's output looks like an HTTP response: headers, blank line, body. We find the
blank line (either `\r\n\r\n` or `\n\n` — both are valid in CGI).

```cpp
if (separator != string::npos)
{
    string headers_part = cgi_output.substr(0, separator);
    body = cgi_output.substr(separator + sep_len);

    stringstream ss(headers_part);
    string line;
    while (getline(ss, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);  // strip trailing \r from \r\n lines
        ...
        size_t colon = line.find(':');
        string key = line.substr(0, colon);
        string val = line.substr(colon + 1);
        // trim whitespace from key and val
        ...
        if (key == "Status")
        {
            stringstream status_ss(val);
            status_ss >> statusCode;       // e.g. 404
            getline(status_ss, statusMsg); // e.g. " Not Found"
            // trim leading whitespace from statusMsg
        }
        else
            cgiHeaders[key] = val;
    }
}
else
    body = cgi_output;   // No headers at all — treat everything as body
```
- CGI scripts can set the HTTP status code by outputting a `Status: 404 Not Found`
  header. This is different from the final HTTP status line — the server translates it.
- All other headers (e.g. `Content-Type: text/html`) are forwarded directly to the client.

```cpp
response.setStatusCode(statusCode);
response.setMessage(statusMsg.empty() ? HttpResponse::getDefaultStatusMessage(statusCode) : statusMsg);

for (...cgiHeaders...)
    response.setResponseHeader(it->first, it->second);

if (cgiHeaders.find("Content-Length") == cgiHeaders.end() &&
    cgiHeaders.find("content-length") == cgiHeaders.end())
{
    response.setResponseHeader("Content-Length", intToString(body.size()));
}
response.setResponseBody(body);
response.serializeResponse(...);
```
If the CGI script didn't output a `Content-Length` header, we calculate and set one
ourselves. We check both `Content-Length` and `content-length` because HTTP headers are
case-insensitive and different scripts may use different casing.

---

## 5. Ready-to-Use Function Comments

These comments are formatted for placement directly above each function in the `.cpp` or
`.hpp` files.

```cpp
/**
 * @brief Main entry point for building an HTTP response.
 *
 * Validates the request, locates the matching configuration block,
 * resolves the target path on disk, and dispatches to the appropriate
 * handler (redirection, autoindex, CGI, static file, upload, or delete).
 *
 * All errors are thrown as HttpException objects and caught by a unified
 * catch block that builds and serializes a proper error response.
 *
 * @param manager The connection context holding the parsed request,
 *                the response to fill, and the virtual-host configuration.
 */
static void build(FdManager &manager);
```

```cpp
/**
 * @brief Finds the most specific location block matching the request URI.
 *
 * Implements the Nginx-style longest-prefix match algorithm: iterates all
 * location blocks in the server config and returns the one whose path is
 * the longest prefix of the request path.
 *
 * @param requestPath The URI path from the HTTP request line (e.g. "/api/v1/users").
 * @param server      The virtual-host configuration containing location blocks.
 * @return Pointer to the best-matching LocationConf, or NULL if none matches.
 */
static const LocationConf* findLocation(const std::string &requestPath, const Server &server);
```

```cpp
/**
 * @brief Generates an HTML directory listing page for a given directory.
 *
 * Opened when autoindex is enabled and no index file was found.
 * Uses opendir/readdir to enumerate entries and stat() to distinguish
 * files from subdirectories (appending "/" to directory names).
 *
 * @param dirPath  Absolute filesystem path to the directory to list.
 * @param uriPath  The URI path displayed in the page title and links.
 * @return HTML string on success; empty string if the directory cannot be opened.
 */
static std::string generateDirectoryListing(const std::string &dirPath, const std::string &uriPath);
```

```cpp
/**
 * @brief Reads an entire file into memory as a binary string.
 *
 * Opens the file with ios::ate to determine its size in one seek,
 * then reads all bytes in a single call. Suitable for text and binary
 * files (images, fonts, videos, etc.).
 *
 * @param filepath Absolute path to the file to read.
 * @param content  Output parameter: receives the file's raw bytes.
 * @return true on success, false if the file could not be opened or read.
 */
static bool readBinaryFile(const std::string &filepath, std::string &content);
```

```cpp
/**
 * @brief Executes a CGI script and parses its output into an HTTP response.
 *
 * Forks a child process, redirecting stdin from a temp file (containing the
 * request body) and stdout into a second temp file. The child executes the
 * interpreter with the script path, receiving CGI environment variables.
 *
 * After the child exits, the parent reads the output file, splits it into
 * CGI headers and body, and populates the HttpResponse accordingly.
 * Temp files are always removed on exit.
 *
 * @param manager       Full connection context (request + response to fill).
 * @param physicalPath  Absolute path to the CGI script on disk.
 * @param interpreter   Path to the interpreter binary (e.g. "/usr/bin/python3").
 *                      If empty, the script is executed directly.
 */
static void executeCGI(FdManager &manager, const std::string &physicalPath, const std::string &interpreter);
```

```cpp
/**
 * @brief Resolves a file extension to its MIME type string.
 *
 * Looks up the extension (e.g. ".html", ".png") in FdManager::extensions,
 * a static map populated at server startup from the configuration.
 *
 * @param path        File path; only the extension is used.
 * @param defaultMime Fallback MIME type if the extension is not recognised.
 * @return The MIME type string (e.g. "text/html", "image/png").
 */
static std::string getMimeType(const std::string &path, const std::string &defaultMime);
```

```cpp
/**
 * @brief Converts an integer to its decimal string representation.
 *
 * Provided because std::to_string() is not available in C++98.
 * Uses a stringstream for the conversion.
 *
 * @param number The integer to convert.
 * @return Decimal string representation (e.g. 42 → "42").
 */
static std::string intToString(int number);
```

---

## 6. Code Issues, Potential Bugs, and Design Notes

| # | Location | Issue | Risk | Recommendation |
|---|----------|-------|------|----------------|
| 1 | `findLocation()` L249 | `const_cast<Server&>` to call a non-const `getLocations()` | Low – logic is correct | Make `Server::getLocations()` a `const` method |
| 2 | `executeCGI()` L336 | `static int cgi_counter` is not thread-safe | Low (single-threaded server) | Use `getpid()` + timestamp for name uniqueness instead |
| 3 | `executeCGI()` L426 | `execve()` in child process uses `envp` which points into `env` — both local to the parent's stack. After `fork()`, the child has its own copy, so this is **safe** | None | Good as-is, just non-obvious |
| 4 | `executeCGI()` L431 | `waitpid()` with `0` flags is blocking — the server freezes while the CGI script runs | High for slow scripts | Should use `WNOHANG` + event-loop integration, or impose a timeout |
| 5 | `executeCGI()` L384 | `toupper()` is called on `char` — if the header contains non-ASCII bytes, this triggers undefined behaviour | Low | Cast to `unsigned char` before `toupper()` |
| 6 | `build()` L89 | The commented-out `/*testIndex.empty() ||*/` is dead code left in the source | Cosmetic | Remove the comment |
| 7 | `build()` L188 | `time(NULL)` for upload filenames is collision-prone if two uploads arrive in the same second | Medium | Append a counter or use `getpid()` |
| 8 | `generateDirectoryListing()` | Entry names are not HTML-escaped. A file named `<script>` would inject JavaScript into the listing page (XSS) | Medium | Run entry names through an HTML-escape function before inserting into `<a href>` |
| 9 | `executeCGI()` | CGI `QUERY_STRING` is extracted from the full path including `?`. But `getPath()` may or may not include the query string depending on the parser | Medium | Verify `HttpRequest::getPath()` behaviour and use a dedicated `getQueryString()` accessor if available |
| 10 | `getMimeType()` | Checks `FdManager::extensions` — if this map is empty (config doesn't define MIME types), every file gets `application/octet-stream`, which forces downloads instead of inline display | Medium | Ship a hardcoded fallback map of common MIME types |
