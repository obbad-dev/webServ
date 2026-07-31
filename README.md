*This project has been created as part of the 42 curriculum by oobbad, yezzemry.*

# webserv

An HTTP/1.1 web server written from scratch in **C++98**, inspired by NGINX. This project implements a fully functional, non-blocking, event-driven web server capable of serving static files, handling file uploads, executing CGI scripts, and managing multiple virtual hosts — all configured through an NGINX-like configuration file.

---

## Description

The goal of **webserv** is to build an HTTP server that is compliant with the HTTP/1.1 protocol (RFC 2616). By writing a web server from the ground up, this project provides deep understanding of network programming, socket management, the HTTP protocol, and concurrent I/O handling — all under the constraint of using only C++98 and the `epoll()` system call for I/O multiplexing.

### Key features

| Feature | Details |
|---|---|
| **HTTP Methods** | `GET`, `POST`, `DELETE` |
| **Static File Serving** | Serves HTML, CSS, JS, images, and other files with proper MIME types |
| **Directory Listing** | Auto-generated index pages when `autoindex` is enabled |
| **File Uploads** | Multipart/form-data file upload with a drag-and-drop web UI |
| **CGI Execution** | Runs CGI scripts (e.g. Python) with proper environment variable setup |
| **Virtual Hosting** | Multiple `server` blocks with `server_name` matching |
| **Custom Error Pages** | Configurable per-server error pages (400, 403, 404, 500…) |
| **HTTP Redirections** | `return` directive for 301/302 redirects |
| **Client Body Limit** | Configurable `client_max_body_size` per server |
| **Non-blocking I/O** | Single-threaded event loop using `epoll()` — no request ever blocks the server |

---

## Architecture

```
src/
├── Program/
│   └── main.cpp                  # Entry point — parses config, starts servers
└── classes/
    ├── parsing/
    │   ├── parseConfig/          # ParseConfig — tokenizes & parses the config file
    │   ├── server/               # Server — stores per-server configuration
    │   └── location/             # Location — stores per-route configuration
    ├── openConnection/           # OpenConnection — socket management & event loop
    ├── http/                     # HTTP — request parsing, routing & response building
    └── helperFunc/               # HelperFunc — MIME types, URL decoding, utilities
```

### Class Responsibilities

| Class | Role |
|---|---|
| `ParseConfig` | Reads the `.conf` file, tokenizes it, and builds a tree of `Server` and `Location` objects |
| `Server` | Data container for server-level directives: `listen`, `server_name`, `root`, `index`, `error_page`, `client_max_body_size`, and its associated `Location` blocks |
| `Location` | Data container for route-level directives: path, `root`, `index`, `allow_methods`, `autoindex`, `return`, `cgi_pass`, `upload_path` |
| `OpenConnection` | Creates listening sockets, runs the `epoll()` event loop, accepts connections, reads requests, dispatches to the HTTP handler, and sends responses |
| `HTTP` | Parses raw HTTP requests, matches URIs to locations, dispatches to GET/POST/DELETE handlers, executes CGI, handles uploads, and builds HTTP responses |
| `HelperFunc` | Static utility functions — MIME type lookup, URL decoding, file/directory checks, string helpers |

---

## Request Lifecycle

The following diagram illustrates the full lifecycle of an HTTP request through the server:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          STARTUP PHASE                                  │
│                                                                         │
│  main() ──► ParseConfig ──► Server[] / Location[] objects               │
│                  │                                                      │
│                  ▼                                                      │
│  OpenConnection::initServer()                                           │
│      • Creates a TCP socket for each server (socket → bind → listen)    │
│      • Adds listening fds to the epoll set                              │
└──────────────────────────────┬──────────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                         EVENT LOOP (epoll)                              │
│                                                                         │
│  OpenConnection::run()                                                  │
│      • Calls epoll() on all file descriptors (listeners + clients)      │
│      • Loops indefinitely, reacting to I/O events                       │
└───────┬─────────────────────────────┬───────────────────────────────────┘
        │ EPOLLIN on listener fd      │ EPOLLIN on client fd
        ▼                             ▼
┌────────────────────┐    ┌──────────────────────────────────────────────┐
│ acceptNewConnection│    │ handleClientData()                           │
│  • accept()        │    │  • read() data incrementally                 │
│  • Add client fd   │    │  • Detect end-of-headers (\r\n\r\n)          │
│    to epoll set    │    │  • Read body via Content-Length / chunked    │
└────────────────────┘    │  • When complete → process request           │
                          └────────────────────┬─────────────────────────┘
                                               │
                                               ▼
                          ┌─────────────────────────────────────────────┐
                          │ matchServer()                               │
                          │  • Select virtual server by Host header     │
                          │    and listening port                       │
                          └────────────────────┬────────────────────────┘
                                               │
                                               ▼
                          ┌─────────────────────────────────────────────┐
                          │ HTTP::handleRequest()                       │
                          │  1. parseRequest() — method, URI, headers   │
                          │  2. matchLocation() — find best Location    │
                          │  3. Validate method & body size             │
                          │  4. Dispatch to handler:                    │
                          │     ┌──────────┬──────────┬──────────┐      │
                          │     │ GET      │ POST     │ DELETE   │      │
                          │     ├──────────┼──────────┼──────────┤      │
                          │     │ Serve    │ Upload   │ Remove   │      │
                          │     │ file or  │ file or  │ file     │      │
                          │     │ autoindex│ exec CGI │          │      │
                          │     └──────────┴──────────┴──────────┘      │
                          │  5. buildResponse() — status, headers, body │
                          └────────────────────┬────────────────────────┘
                                               │
                          ┌────────────────────┘
                          │       If CGI:
                          │   ┌──────────────────────────────────────┐
                          │   │ executeCGI()                         │
                          │   │  • fork() child process              │
                          │   │  • Set env vars (PATH_INFO,          │
                          │   │    QUERY_STRING, CONTENT_TYPE, etc.) │
                          │   │  • Pipe request body to stdin        │
                          │   │  • Capture stdout for response       │
                          │   │  • waitpid() for child               │
                          │   └──────────────────────────────────────┘
                          │
                          ▼
                          ┌─────────────────────────────────────────────┐
                          │ sendResponse()                              │
                          │  • Write response to client socket          │
                          │  • Handle partial sends across epoll cycles │
                          │    (large responses sent via EPOLLOUT)      │
                          └────────────────────┬────────────────────────┘
                                               │
                                               ▼
                          ┌─────────────────────────────────────────────┐
                          │ closeConnection()                           │
                          │  • Remove fd from epoll set                 │
                          │  • close() the socket                       │
                          │  • Clean up client state                    │
                          └─────────────────────────────────────────────┘
```

### Step-by-step summary

1. **Startup** — `main()` reads the configuration file through `ParseConfig`, which produces a vector of `Server` objects, each holding its `Location` routes.
2. **Socket creation** — `OpenConnection::initServer()` creates one TCP listening socket per server block (`socket` → `bind` → `listen`) and registers them in the `epoll` set.
3. **Event loop** — `OpenConnection::run()` calls `epoll()` in an infinite loop, reacting to I/O readiness on all file descriptors.
4. **Accept** — When a listening socket signals `EPOLLIN`, `acceptNewConnection()` calls `accept()` and adds the new client fd to the epoll set.
5. **Read** — When a client socket signals `EPOLLIN`, `handleClientData()` incrementally reads the request. It detects the end of headers (`\r\n\r\n`) and then reads the body according to `Content-Length` or chunked transfer encoding.
6. **Server matching** — Once the request is fully received, `matchServer()` selects the correct virtual server based on the `Host` header and port.
7. **Request handling** — `HTTP::handleRequest()` parses the raw request, matches the URI to a `Location`, validates the method and body size, then dispatches to `handleGet()`, `handlePost()`, or `handleDelete()`.
8. **GET** — Resolves the file path on disk, reads the file, determines the MIME type, and returns it. If the path is a directory and `autoindex` is enabled, generates an HTML directory listing.
9. **POST** — If the request is `multipart/form-data`, parses boundaries and saves uploaded files. If the matched location has CGI configured, forks a child process to execute the script.
10. **DELETE** — Removes the requested file from the filesystem and returns a success response.
11. **CGI** — `executeCGI()` forks, sets environment variables (`PATH_INFO`, `QUERY_STRING`, `CONTENT_TYPE`, `CONTENT_LENGTH`, `REQUEST_METHOD`, etc.), pipes the request body to the script's stdin, captures stdout, and parses CGI-produced headers.
12. **Response** — `buildResponse()` assembles the HTTP response (status line, headers, body). `sendResponse()` writes it to the client socket, handling partial sends across multiple `epoll()` iterations.
13. **Cleanup** — After the response is fully sent, `closeConnection()` removes the fd from the epoll set and closes the socket.

---

## Instructions

### Prerequisites

- A Unix-like operating system (Linux / macOS)
- A C++ compiler supporting C++98 (`c++`, `g++`, or `clang++`)
- `make`
- Python 3 (for CGI script execution)

### Compilation

```bash
# Clone the repository
git clone <repository-url> webserv
cd webserv

# Build the project
make

# Other Makefile targets:
make clean    # Remove object files
make fclean   # Remove object files and the binary
make re       # Full rebuild
```

The binary `webServ` will be created in the project root.

### Running the server

```bash
# Run with the default configuration
./webServ

# Run with a custom configuration file
./webServ resources/configFiles/default.conf
```

The server will start listening on the ports defined in the configuration file (default: `8080`). Open your browser and navigate to `http://localhost:8080`.

### Configuration

The server is configured via an NGINX-inspired `.conf` file. Here is an example:

```nginx
server {
    listen 8080;
    server_name localhost;
    root ./resources/sites/;
    client_max_body_size 10m;

    error_page 404 /errors/404.html;

    location / {
        allow_methods GET;
        index index.html;
        autoindex off;
    }

    location /upload {
        allow_methods GET POST DELETE;
        enable_upload on;
        upload_path ./uploads/;
    }

    location /cgi {
        allow_methods GET POST;
        cgi_pass .py /usr/bin/python3;
    }

    location /old-page {
        return 301 /new-page;
    }
}
```

#### Available directives

| Directive | Scope | Description |
|---|---|---|
| `listen` | server | Port (or host:port) to listen on |
| `server_name` | server | Virtual host name(s) |
| `root` | server / location | Root directory for serving files |
| `index` | server / location | Default file(s) to serve for directories |
| `error_page` | server | Custom error page for given status code(s) |
| `client_max_body_size` | server | Maximum allowed request body size (e.g. `10m`, `1k`) |
| `allow_methods` | location | Allowed HTTP methods (`GET`, `POST`, `DELETE`) |
| `autoindex` | location | Enable (`on`) or disable (`off`) directory listing |
| `return` | location | HTTP redirection (`return 301 /target`) |
| `cgi_pass` | location | CGI extension and interpreter path (`.py /usr/bin/python3`) |
| `enable_upload` | location | Enable file upload handling (`on` / `off`) |
| `upload_path` | location | Directory where uploaded files are stored |

### Testing

The project includes a tester binary and a CGI tester:

```bash
# Run the provided tester (make sure the server is running with tester.conf)
./webServ resources/configFiles/tester.conf &
./tester http://localhost:8080

# Run the CGI tester
./cgi_tester
```

You can also test manually with `curl`:

```bash
# GET request
curl -v http://localhost:8080/

# POST file upload
curl -X POST -F "file=@test.txt" http://localhost:8080/upload

# DELETE a file
curl -X DELETE http://localhost:8080/upload/test.txt
```

---

## Technical Choices

| Decision | Rationale |
|---|---|
| **C++98** | Required by the 42 project subject — no C++11 or later features |
| **`epoll()` for I/O multiplexing** | Chosen over `select()` for better scalability (no fd limit), and over `epoll` for portability across Unix systems |
| **Single-threaded architecture** | Simplifies state management and avoids race conditions; the non-blocking event loop handles concurrency |
| **NGINX-style configuration** | Familiar syntax for anyone with web server experience; hierarchical server/location blocks provide flexible routing |
| **Fork-based CGI** | Standard CGI model — each CGI request forks a child process with environment variables, matching the CGI/1.1 specification |

---

## Project Structure

```
webServ/
├── Makefile                          # Build system
├── README.md                         # This file
├── resources/
│   ├── configFiles/
│   │   ├── default.conf              # Default server configuration
│   │   └── tester.conf               # Tester configuration
│   └── sites/
│       ├── index.html                # Landing page
│       ├── style.css                 # Site stylesheet
│       ├── upload.html               # Drag & drop upload interface
│       └── cgi/                      # CGI scripts (Python)
├── src/
│   ├── Program/
│   │   └── main.cpp                  # Entry point
│   └── classes/
│       ├── parsing/
│       │   ├── parseConfig/          # Config file parser
│       │   ├── server/               # Server configuration class
│       │   └── location/             # Location/route configuration class
│       ├── openConnection/           # Socket & event loop management
│       ├── http/                     # HTTP protocol handling
│       └── helperFunc/               # Utility functions
├── uploads/                          # Default upload directory
├── YoupiBanane/                      # Test directory for tester
├── tester                            # Provided test binary
└── cgi_tester                        # Provided CGI test binary
```

---

## Resources

### References & Documentation

- [RFC 2616 — HTTP/1.1](https://datatracker.ietf.org/doc/html/rfc2616) — The HTTP/1.1 specification that this project implements.
- [RFC 3875 — The Common Gateway Interface (CGI) Version 1.1](https://datatracker.ietf.org/doc/html/rfc3875) — The CGI specification used for script execution.
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/) — Comprehensive guide to socket programming in C.
- [NGINX Documentation](https://nginx.org/en/docs/) — Reference for configuration syntax and server behavior that inspired this project.
- [The `poll()` System Call — Linux man page](https://man7.org/linux/man-pages/man7/epoll.7.html) — Documentation for the I/O multiplexing mechanism used.
- [Mozilla HTTP Reference](https://developer.mozilla.org/en-US/docs/Web/HTTP) — General HTTP protocol reference and status codes.

### AI Usage Disclosure

AI tools (GitHub Copilot, ChatGPT) were used during the development of this project for the following tasks:

- **Debugging assistance** — Identifying issues in socket setup, HTTP parsing edge cases, and CGI environment variable configuration.
- **Code review** — Reviewing code structure and suggesting improvements for error handling and edge cases.
- **Documentation** — Assisting in drafting parts of this README and configuration examples.

All core logic, architecture decisions, and implementation were designed and written by the team members. AI was used as a supplementary tool, not as a primary code generator.
