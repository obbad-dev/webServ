# WebServ HTTP Response Construction Workflow

This document outlines the end-to-end logical workflow for building HTTP/1.1 responses in a C++98 web server using raw sockets. It is designed to guide you from receiving the request to pushing the final bytes over the socket.

## 1. High-Level Flow (Mermaid Scheme)

```mermaid
flowchart TD
    A[Client Socket Ready for Read] -->|recv()| B(Request Parser)
    B --> C{Request Complete?}
    C -->|No| A
    C -->|Yes| D(Router & Config Matcher)
    D --> E{Target Type?}
    
    E -->|Static File| F[Static Handler: GET/POST/DELETE]
    E -->|Directory| G[Directory Handler: Autoindex or Index File]
    E -->|CGI Script| H[CGI Handler: fork, execve, pipe]
    
    F --> I(Response Builder)
    G --> I
    H -->|Parse CGI Output| I
    
    I --> J[Serialize Response Buffer]
    J --> K[Client Socket Ready for Write]
    K -->|send()| L{All Bytes Sent?}
    L -->|No| K
    L -->|Yes| M[Keep-Alive or Close]
```

## 2. The HTTP Response Structure

Before diving into specific methods, remember that **every** HTTP response must strictly follow this format, separated by Carriage Return Line Feed (`\r\n`):

```http
[HTTP Version] [Status Code] [Status Message]\r\n
[Header Key 1]: [Header Value 1]\r\n
[Header Key 2]: [Header Value 2]\r\n
\r\n
[Response Body (Optional)]
```

**Example:**
```http
HTTP/1.1 200 OK\r\n
Content-Length: 45\r\n
Content-Type: text/html\r\n
Connection: keep-alive\r\n
\r\n
<html><body><h1>Hello World</h1></body></html>
```

## 3. Step-by-Step Response Construction Workflows

### Scenario A: Static File (GET)

1. **Path Resolution**: Router gives you an absolute path (e.g., `/var/www/html/image.png`).
2. **Validation (`stat`)**:
   - Check if file exists. If not -> Build `404 Not Found` response.
   - Check permissions (`access`). If no read rights -> Build `403 Forbidden` response.
3. **MIME Type Detection**: Extract the file extension (`.png`). Look it up in your configuration (e.g., map `png` to `image/png`).
4. **Read File**:
   - Open file (`open` or `std::ifstream`).
   - Read size (using `stat.st_size` or by seeking to end).
   - Read content into a `std::string` or `std::vector<char>`.
5. **Build Response**:
   - Status Line: `HTTP/1.1 200 OK`
   - Headers: `Content-Type: image/png`, `Content-Length: <size>`
   - Body: Raw file bytes.

### Scenario B: Directory Listing (GET with Autoindex)

1. **Path Resolution**: Router gives you a directory path (e.g., `/var/www/html/docs/`).
2. **Index Check**: Check if `index.html` exists inside. If yes, treat it as Scenario A.
3. **Generate Autoindex**:
   - Open directory using `opendir()`.
   - Read entries using `readdir()`.
   - Start building an HTML string: `<html><body><h1>Index of /docs/</h1><ul>`
   - Loop entries: append `<li><a href="filename">filename</a></li>` to the HTML string.
   - Close tags: `</ul></body></html>`.
4. **Build Response**:
   - Status Line: `HTTP/1.1 200 OK`
   - Headers: `Content-Type: text/html`, `Content-Length: <size of generated string>`
   - Body: Generated HTML string.

### Scenario C: Static Method (DELETE)

1. **Path Resolution**: Target file path.
2. **Validation**: Check if it exists (`404` if not) and check server permissions (`403` if not allowed).
3. **Deletion**: Execute `unlink(target_path.c_str())`.
   - If `unlink` fails -> Build `500 Internal Server Error`.
4. **Build Response**:
   - Status Line: `HTTP/1.1 204 No Content` (Best practice when returning no body) or `HTTP/1.1 200 OK` (If you want to send a success HTML page).
   - Headers: `Content-Length: 0` (if 204).
   - Body: None (if 204).

### Scenario D: File Upload (POST without CGI)

*Note: Depending on your project requirements, uploads might exclusively be handled by CGI. If direct HTTP uploads are required:*
1. **Header Parsing**: Read `Content-Length` and optionally handle `Transfer-Encoding: chunked`.
2. **File Creation**: Open target file with `O_WRONLY | O_CREAT | O_TRUNC`.
3. **Write Body**: Write the request body to the file.
4. **Build Response**:
   - Status Line: `HTTP/1.1 201 Created`
   - Headers: `Location: /uploads/newfile.txt`, `Content-Length: 0`
   - Body: None.

### Scenario E: CGI Execution (GET/POST)

This is a two-step process: (1) Execution, (2) Output Parsing.

**Step 1: Execution (fork/execve/pipe)**
- Create pipes (Server->CGI for body, CGI->Server for output).
- `fork()`.
- **Child**: dup2 pipes to `stdin`/`stdout`, set up `envp` (e.g., `REQUEST_METHOD`, `QUERY_STRING`), `execve()` the script.
- **Parent**: write body to pipe (if POST), read output from pipe into a buffer, `waitpid`.

**Step 2: Output Parsing & Response Building**
The buffer read from the CGI's `stdout` will look something like this:
```text
Content-type: application/json\r\n
Status: 400 Bad Request\r\n        <-- CGI might set its own status!
\r\n
{"error": "Invalid format"}
```

You **cannot** just send this directly to the client socket, because it lacks the `HTTP/1.1` status line and standard headers.

1. **Find Separator**: Locate `\r\n\r\n` in the CGI output. Everything before is headers, everything after is the body.
2. **Parse CGI Headers**:
   - Look for a `Status:` header. If found, extract it (e.g., `400 Bad Request`). If not found, default to `200 OK`.
   - Look for `Content-type`.
3. **Calculate Length**: Calculate the size of the CGI body portion.
4. **Merge & Build Response**:
   - Status Line: `HTTP/1.1 <Status Code> <Status Message>` (Derived from step 2).
   - Server Headers: Add `Content-Length: <size>`, `Server: MyWebServ`.
   - CGI Headers: Append the rest of the CGI headers (excluding `Status:` if you extracted it).
   - Empty Line: `\r\n`.
   - Body: The CGI body portion.

## 4. Sending the Response (`send`)

Once your Response Builder outputs a complete raw buffer (`std::string` or `std::vector<char>`), it's time to send it over the wire.

1. **Register for Write**: Wait until `select()` or `epoll` or `poll()` tells you the client socket is ready for writing.
2. **Send Chunk**: Call `bytes_sent = send(client_fd, buffer.data() + total_sent, buffer.size() - total_sent, 0);`.
3. **Partial Sends**: Sockets have internal limits. A single `send()` might not write the whole response at once. You must track `total_sent`. If `total_sent < buffer.size()`, return to your event loop and wait for the socket to be ready to write again.
4. **Cleanup**: Once `total_sent == buffer.size()`, check if the request was `Connection: close`. If so, close the socket. If `Connection: keep-alive`, clear the client's request state and wait for the next request on the same socket.
