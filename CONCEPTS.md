# Webserv — Core Concepts: POST, DELETE, and CGI

> A mentor's walkthrough, grounded **strictly** in what the webserv subject
> (Version 23.1) actually asks for. The goal is that you understand the *why*
> and *how* well enough to design your own implementation — not copy one.
>
> Reference point the subject gives you: **HTTP/1.0 is suggested** ("not
> enforced"), and you're told to *read the RFCs, test with `telnet` and NGINX*
> before writing code. Everything below is written in that spirit: understand
> the protocol, then build a subset of it.

---

## Table of Contents

1. [Ground rules the subject imposes (read this first)](#0)
2. [Concept 1 — What "building a response" even means](#1)
3. [Concept 2 — The POST method](#2)
4. [Concept 3 — The DELETE method](#3)
5. [Concept 4 — CGI from scratch](#4)
6. [Concept 5 — The full request→response workflow (text diagram)](#5)
7. [Self-check questions before you code](#6)

---

<a name="0"></a>
## 0. Ground rules the subject imposes (read this first)

Before we talk about any single method, internalize the constraints the subject
puts on *every* response you ever produce. These shape all the design decisions
below.

- **You write an HTTP server in C++98.** Arguments: `./webserv [config file]`.
- **Everything is non-blocking, driven by one `poll()` (or equivalent).** You
  must *never* `read`/`recv` or `write`/`send` on a socket/pipe without `poll()`
  first telling you it's ready. This includes the pipes you'll use to talk to a
  CGI process. Regular disk files are the *only* exception.
- **You may not check `errno` after a read/write** to decide what to do. This
  means: you can't "try a read and see if it would block." You must rely on
  `poll()` readiness instead. Design your state machine around this.
- **`fork()` is forbidden for everything except CGI.** So the only place a child
  process appears in your whole program is CGI execution.
- **HTTP status codes must be accurate**, and you must have **default error
  pages** when the config doesn't provide one.
- **The server must never crash and never hang indefinitely.** Every response
  path — including the error paths — must terminate cleanly.
- **Config drives behavior.** Per-route (`location`) you can configure: allowed
  methods, redirection, root directory, directory listing on/off, default file,
  upload on/off + storage location, and CGI by file extension. A method that
  isn't in `allow_methods` for the matched route must be *rejected*, not
  executed.

Keep these in the back of your mind for every example that follows.

---

<a name="1"></a>
## 1. Concept 1 — What "building a response" even means

An HTTP response is just **structured text** (plus an optional raw body) that you
write back onto the client socket. It has exactly four parts, in this order:

```
<status-line>\r\n          ← HTTP version + status code + reason phrase
<header>: <value>\r\n       ← zero or more header lines
<header>: <value>\r\n
\r\n                        ← one BLANK line: "headers are done"
<body bytes>                ← optional payload (HTML, uploaded file, CGI output…)
```

Three things that trip everyone up, so lock them in now:

1. **Line endings are CRLF (`\r\n`), not `\n`.** Every line, including the blank
   separator line. A single missing `\r` will make browsers and NGINX-comparison
   tests behave strangely.
2. **The blank line is mandatory and is the boundary** between headers and body.
   The client counts on it.
3. **The body length must be knowable.** In HTTP/1.0 style (your reference), the
   normal way is a `Content-Length` header telling the client exactly how many
   bytes of body follow. (The alternative — "read until the connection closes" —
   also exists, and as you'll see, it's exactly how CGI output can end.)

A minimal, correct response looks like:

```
HTTP/1.1 200 OK\r\n
Content-Type: text/html\r\n
Content-Length: 48\r\n
\r\n
<html><body><h1>Hello, webserv</h1></body></html>
```

**Mental model for the whole project:** parse the request → decide *what*
the client asked for and *whether they're allowed* → produce a status code +
headers + body → serialize it as the text above → write it out through `poll()`.
POST, DELETE, and CGI are just three different ways of deciding the *what*.

---

<a name="2"></a>
## 2. Concept 2 — The POST method

### 2.1 What POST is *supposed* to do

The subject's Introduction says it plainly: HTTP mostly serves content, but it
**also enables clients to send data — used for submitting web forms, including
the uploading of files.** POST is the method for "here is a body of data, do
something with it on the server."

For your mandatory scope, POST has two realistic jobs:

- **Uploading a file** (the subject explicitly requires: *"Clients must be able
  to upload files"*). The client sends bytes in the request body; you save them
  to the route's configured upload directory.
- **Feeding a body to a CGI script** (e.g. a form posted to a `.py`/`.php`
  handler). Here you don't store the body yourself — you hand it to the CGI
  process on its stdin. (Covered in Concept 4.)

The defining trait of POST vs GET: **POST carries a request body**, and that body
is the point. So the heart of POST handling is *reading the body correctly*.

### 2.2 Anatomy of a POST request

```
POST /uploads/photo.png HTTP/1.1\r\n
Host: localhost:8080\r\n
Content-Type: image/png\r\n
Content-Length: 20481\r\n
\r\n
<20481 raw bytes of the PNG file>
```

Two ways the client can tell you how long the body is — **you must handle both**:

- **`Content-Length: N`** — read exactly `N` bytes of body. Simple case.
- **`Transfer-Encoding: chunked`** — the body arrives in size-prefixed chunks and
  there is *no* up-front length. The subject calls this out directly: *"for
  chunked requests, your server needs to un-chunk them."* A chunked body looks
  like:

  ```
  1a\r\n                       ← chunk size in HEX (0x1a = 26 bytes)
  <26 bytes of data>\r\n
  10\r\n                       ← next chunk, 0x10 = 16 bytes
  <16 bytes of data>\r\n
  0\r\n                        ← a zero-size chunk = "body is finished"
  \r\n
  ```

  Your job: read each hex size, read that many bytes, repeat until you hit the
  `0`-size chunk, and reassemble the pieces into one continuous body. That
  reassembled body is what you then store (or feed to CGI).

### 2.3 Step-by-step: handling a POST that stores an upload

**Step 1 — Route resolution.** Match the request path (`/uploads/photo.png`)
against your `location` blocks. Suppose it matches:

```nginx
location /uploads {
    allow_methods POST;
    enable_upload on;
    upload /var/www/mySite/uploads;
}
```

**Step 2 — Is the method allowed here?** `allow_methods` for this route lists
`POST`. Good. If a route's `allow_methods` did *not* include POST, you stop
immediately and respond **`405 Method Not Allowed`** — and per HTTP you should
add an `Allow:` header listing what *is* permitted. Never execute a
disallowed method.

**Step 3 — Read the body under the size limit.** The subject requires a
configurable **`client_max_body_size`**. As you accumulate body bytes (whether
from `Content-Length` or by un-chunking), if the total exceeds the limit, stop
and respond **`413 Payload Too Large`** (a.k.a. "Content Too Large"). This
protects you against a client trying to exhaust your memory — resilience is a
graded requirement.

**Step 4 — Persist the bytes.** Write the assembled body to the upload directory
(`/var/www/mySite/uploads/photo.png`). Consider: does the target directory
exist and is it writable? If not, that's a **`500`**-class failure or a
permissions problem — surface it honestly with the right code, don't pretend it
worked.

**Step 5 — Build the success response.** Choose a status code that matches what
happened:

- **`201 Created`** — a new resource was created (a new file was stored). This is
  the most semantically correct code for a successful upload, and you should
  include a **`Location`** header pointing at where the new resource lives.
- **`200 OK`** — acceptable if you're returning a result/confirmation page and
  don't want to assert "created."
- **`204 No Content`** — you succeeded and there is deliberately *no* body to
  return. If you use 204, you send **no body and no `Content-Length`**.

### 2.4 Concrete example — request and a correct response

Request the browser/`telnet` sends:

```
POST /uploads/photo.png HTTP/1.1
Host: localhost:8080
Content-Type: image/png
Content-Length: 20481

<20481 raw bytes>
```

A correct response you build:

```
HTTP/1.1 201 Created\r\n
Location: /uploads/photo.png\r\n
Content-Type: text/html\r\n
Content-Length: 62\r\n
\r\n
<html><body><p>Upload stored: /uploads/photo.png</p></body></html>
```

### 2.5 Edge cases you must watch for with POST

| # | Situation | Correct behavior |
|---|-----------|------------------|
| 1 | Method not in the route's `allow_methods` | `405 Method Not Allowed` + `Allow:` header. Do **not** process the body. |
| 2 | Body larger than `client_max_body_size` | `413`. Ideally stop reading early rather than buffering it all. |
| 3 | `Transfer-Encoding: chunked` | Un-chunk before you do anything with the body. A `0\r\n\r\n` terminates it. |
| 4 | Neither `Content-Length` nor chunked, but a body is implied | Ambiguous/malformed → treat as `400 Bad Request` (or read as empty body). Decide a rule and be consistent. |
| 5 | `Content-Length` says N but the client sends fewer bytes then stalls | You must **not hang forever** — the subject forbids indefinite hangs. Have a timeout / disconnect policy. |
| 6 | Upload directory missing / not writable | Don't crash; return `500` (or `403` if it's a permission issue) with an error page. |
| 7 | Upload disabled for this route (`enable_upload off`) | Don't store. Either `403 Forbidden` or route to whatever else that location does. |
| 8 | POST body must arrive across many `poll()` wake-ups | The body may not all arrive in one `recv`. You need a per-connection state machine that *accumulates* until complete, never blocking. |
| 9 | Client disconnects mid-upload | Detect it (a `recv` returning 0 = orderly close), free the connection cleanly. |

The recurring theme: **read the body incrementally and defensively.** That's 80%
of what makes POST hard in a non-blocking server.

---

<a name="3"></a>
## 3. Concept 3 — The DELETE method

### 3.1 What DELETE is *supposed* to do

DELETE means: **"remove the resource identified by this URL from the server."**
In your scope, the "resource" is a file (or possibly a directory) under the
route's root. It is the counterpart to POST/upload: POST puts data on the server,
DELETE takes it off.

DELETE has **no meaningful request body** — the target is the URL itself. So
unlike POST, the hard part isn't reading input; it's **resolving the path safely
and reporting the outcome with the right status code.**

### 3.2 Step-by-step: handling a DELETE

**Step 1 — Route resolution + method check.** Same as POST: match the
`location`, and verify `DELETE` is in that route's `allow_methods`. If not →
**`405 Method Not Allowed`** with an `Allow:` header. Stop.

**Step 2 — Map the URL to a filesystem path.** Using the route's `root` (and the
path-rooting rule the subject gives: if `/kapouet` is rooted to `/tmp/www`, then
`/kapouet/pouic/toto/pouet` maps to `/tmp/www/pouic/toto/pouet`), compute the
real file path to delete.

**Step 3 — Guard against path traversal.** This is a security must. A malicious
`DELETE /../../etc/passwd` must **not** escape the configured root. Normalize the
path and reject anything that climbs above the root → `403 Forbidden` (or `400`).
The subject stresses your server must be *resilient*; this is part of that.

**Step 4 — Check existence and permission (`access`, `stat`).**

- Doesn't exist → **`404 Not Found`**.
- Exists but you can't remove it (permissions, or it's a non-empty directory you
  won't recurse into) → **`403 Forbidden`**.

**Step 5 — Perform the deletion and choose the status code:**

- **`204 No Content`** — deletion succeeded and you're returning no body. This is
  the cleanest and most common choice for DELETE. No body, no `Content-Length`.
- **`200 OK`** — deletion succeeded *and* you want to return a small confirmation
  body (e.g. an HTML "deleted" page). Then you *do* include a body +
  `Content-Length`.
- **`202 Accepted`** — only if deletion is queued/deferred rather than done
  immediately (unlikely in your scope; mentioned for completeness).

### 3.3 Concrete example — request and a correct response

Request:

```
DELETE /uploads/photo.png HTTP/1.1
Host: localhost:8080

```

Correct response (nothing left to return):

```
HTTP/1.1 204 No Content\r\n
\r\n
```

Or, if you prefer to confirm with a page:

```
HTTP/1.1 200 OK\r\n
Content-Type: text/html\r\n
Content-Length: 40\r\n
\r\n
<html><body><p>Deleted.</p></body></html>
```

### 3.4 Edge cases you must watch for with DELETE

| # | Situation | Correct behavior |
|---|-----------|------------------|
| 1 | Target file doesn't exist | `404 Not Found` |
| 2 | DELETE not in route's `allow_methods` | `405 Method Not Allowed` + `Allow:` |
| 3 | Path traversal attempt (`..`) escaping root | `403 Forbidden` (or `400`); never delete outside root |
| 4 | No write/remove permission on the file | `403 Forbidden` |
| 5 | Target is a directory | Decide a policy: refuse (`403`), or only delete if empty. Be consistent and don't crash. |
| 6 | Deletion partially fails / OS error | `500 Internal Server Error` with an error page |
| 7 | `204` chosen but you accidentally send a body | Bug: `204` must have **no** body and **no** `Content-Length`. Clients may hang or mis-parse. |
| 8 | Client sends a body with DELETE | Ignore it, but you must still *consume/skip* it so the connection stays in a clean state for keep-alive. |

### 3.5 POST vs DELETE at a glance

| | POST | DELETE |
|---|------|--------|
| Purpose | Send data to server (upload / CGI input) | Remove a resource |
| Request body | Central — read it carefully | Usually none |
| Hard part | Reading body (length/chunked) non-blocking | Safe path resolution + honest status |
| Typical success code | `201 Created` (or `200`/`204`) | `204 No Content` (or `200`) |
| Common failures | `405`, `413`, `400`, `500` | `404`, `403`, `405`, `500` |

---

<a name="4"></a>
## 4. Concept 4 — CGI from scratch

The subject asks pointedly: *"Do you wonder what a CGI is?"* — so let's build the
whole idea up from zero.

### 4.1 The problem CGI solves (the *why*)

Everything so far has been about **static** content: a file exists on disk, you
read it, you send it. But the Introduction says the resource can be *"the result
of a program... and can actually be many other things."* Sometimes the response
must be **generated at request time** — a form result, a page that reads a
database, output that depends on *this specific* request.

Your web server is written in C++98 and knows nothing about PHP or Python. You do
**not** want to build a PHP interpreter into your server. So how do you serve a
`.php` or `.py` page?

**CGI (Common Gateway Interface)** is the answer: a *standard contract* for a web
server to hand a request off to an **external program** (the "CGI script"),
let that program produce the dynamic output, and read that output back as the
response. It's called an *interface* because it standardizes **how the server and
the script talk to each other**, regardless of what language the script is in.

That contract has two halves:

1. **Server → script:** the request info is delivered via **environment
   variables** (metadata) and the request **body via stdin** (data).
2. **Script → server:** the script writes its output (a few headers + a body) to
   **stdout**, and the server reads it back.

This is exactly why the subject says you may only `fork()` *for CGI*: running an
external program is inherently a separate-process job, and CGI is the one place
that's legitimate.

### 4.2 The tools the subject hands you (map them to their roles)

Look at the allowed functions — they're basically a CGI starter kit:

| Function(s) | Role in CGI |
|-------------|-------------|
| `fork` | Create a child process to become the script |
| `execve` | In the child, replace it with the interpreter (`python3`, `php-cgi`) |
| `pipe` | Create the two one-way tubes: one for the body in, one for output out |
| `dup2` | Rewire the child's stdin/stdout onto those pipes |
| `waitpid` | Reap the child, learn its exit status |
| `chdir` | Run the script in the *correct directory* (subject requires this) |
| `access`, `stat` | Check the script exists / is executable before running |
| `read`, `write`, `close` | Move bytes across the pipes and clean up |
| `poll`/equivalent | Watch the pipe FDs for readiness — **required**, same as sockets |

Notice there's no `putenv`/`setenv` in the list — you build the environment as an
array of `"KEY=VALUE"` C-strings and pass it as the third argument to `execve`.
That array **is** the "set environment variables" step.

### 4.3 Environment variables — the server→script metadata

This is the part the subject tells you to study most carefully: *"Have a careful
look at the environment variables involved... The full request and arguments
provided by the client must be available to the CGI."*

The CGI script cannot see your sockets or your parsed request struct. The *only*
way it learns about the request is the environment you construct. So your job is
to translate the parsed HTTP request into the standard CGI variable names. A
representative set for a POST to a Python script:

```
# --- Who/what/where ---
REQUEST_METHOD=POST                 ← the HTTP method
SCRIPT_NAME=/api/form.py            ← the script's URL path
SCRIPT_FILENAME=/var/www/api/form.py← real filesystem path to the script
PATH_INFO=/extra/path              ← any path AFTER the script name (if any)
QUERY_STRING=user=ahmed&id=7        ← everything after '?' in the URL (for GET-style params)

# --- About the body being sent on stdin ---
CONTENT_LENGTH=27                   ← how many body bytes the script should read from stdin
CONTENT_TYPE=application/x-www-form-urlencoded

# --- Server / protocol identity ---
SERVER_PROTOCOL=HTTP/1.1
SERVER_NAME=localhost
SERVER_PORT=8080
GATEWAY_INTERFACE=CGI/1.1           ← declares which CGI contract version you speak
REDIRECT_STATUS=200                 ← php-cgi specifically refuses to run without this

# --- The client's headers, re-exposed ---
# Convention: header "Foo-Bar: x"  →  HTTP_FOO_BAR=x  (uppercase, '-'→'_', prefix HTTP_)
HTTP_HOST=localhost:8080
HTTP_USER_AGENT=curl/8.0
HTTP_COOKIE=session=abc123          ← this is how cookies reach a CGI (relevant to the bonus)
```

The key insight: **`CONTENT_LENGTH` and stdin work together.** You tell the
script "there are 27 bytes of body," and the script reads exactly that many bytes
from its stdin. That's the whole handshake for the request body.

Two subject-specific notes fold in here:

- **Chunked in, un-chunked to CGI.** If the client used
  `Transfer-Encoding: chunked`, you un-chunk *first* (Concept 2.2), then set
  `CONTENT_LENGTH` to the assembled length before launching CGI. The CGI expects
  a plain body ending at **EOF** — which is what closing the write end of the
  stdin pipe gives it.
- **`chdir` to the script's directory.** *"The CGI should be run in the correct
  directory for relative path file access."* If `form.py` opens `./data.txt`, it
  must resolve relative to the script's own folder, so you `chdir` there in the
  child before `execve`.

### 4.4 The output contract — script→server (the *how it comes back*)

The script writes to stdout something that looks like a *partial* HTTP response:
a few headers, a blank line, then the body. A typical CGI output:

```
Content-Type: text/html\r\n
\r\n
<html><body><h1>Hello ahmed</h1></body></html>
```

Sometimes it sets a status explicitly:

```
Status: 302 Found\r\n
Location: /welcome.html\r\n
\r\n
```

**How does the server know the body ended?** The subject spells out the exact
rule: *"If no `content_length` is returned from the CGI, EOF will mark the end of
the returned data."* In other words:

- The CGI usually does **not** send a `Content-Length`. So you keep reading its
  stdout until you hit **EOF** — which happens when the script exits and the OS
  closes the pipe's write end. That EOF *is* the "body is complete" signal.
- Your server then takes the CGI's headers, adds/normalizes whatever's needed
  (notably a real HTTP **status line**, and a `Content-Length` computed from the
  bytes you collected, since browsers want one), and sends the whole thing to the
  client as a proper HTTP response.

So the server acts as a **translator**: CGI-style output in → valid HTTP response
out.

### 4.5 Step-by-step CGI execution flow

**Step 1 — Decide it's a CGI request.** Route matched a `cgi_pass .py
/usr/bin/python3`, and the target ends in `.py`. So instead of serving the file
statically, you execute it.

**Step 2 — Sanity checks.** Does the script file exist and is it accessible
(`access`/`stat`)? Does the configured interpreter exist? If not → `404` or
`500`, *without* forking.

**Step 3 — Build the environment array** (Section 4.3) and, if there's a request
body, have it un-chunked and ready.

**Step 4 — Create two pipes.**
- `pipe_in`  — server **writes** the request body → child **reads** it as stdin.
- `pipe_out` — child **writes** its output → server **reads** it as the response.

**Step 5 — `fork()`.**

*In the child:*
- `dup2(pipe_in[read], STDIN_FILENO)` — child's stdin now comes from the pipe.
- `dup2(pipe_out[write], STDOUT_FILENO)` — child's stdout now goes to the pipe.
- `close()` all the unused/duplicated pipe FDs.
- `chdir()` into the script's directory.
- `execve(interpreter, {interpreter, script_path, NULL}, envp)` — become the
  interpreter running the script with your environment. If `execve` fails, the
  child must exit with an error code (so the parent can turn that into `500`).

*In the parent (your server):*
- Close the ends it doesn't use (`pipe_in[read]`, `pipe_out[write]`).
- **Write the request body** into `pipe_in[write]`, then **close it** — that
  close is what delivers EOF to the script's stdin so it knows the body ended.
- **Read the script's output** from `pipe_out[read]` until EOF.

**Step 6 — Do it all through `poll()`, non-blocking.** The pipe FDs are exactly
the "pipes/FIFOs" the subject warns about: they must be non-blocking and driven
by your single `poll()` loop, just like client sockets. You **cannot** just
`write()` the whole body and `read()` the whole output in a blocking loop — a
script that's slow to drain its stdin, or slow to produce output, would freeze
your entire server. Register the pipe FDs with `poll()`, write when writable,
read when readable.

**Step 7 — Reap and guard.** `waitpid` the child to avoid zombies and get its
exit status. Because *"a request must never hang indefinitely"* and *"the server
must remain operational at all times,"* you need a **timeout**: if the CGI runs
too long or hangs, `kill` it and return `504 Gateway Timeout` (or `500`). A
crashed/non-zero CGI → `502 Bad Gateway` / `500`.

**Step 8 — Assemble the HTTP response.** Parse the headers the CGI emitted, set
the final status line, compute `Content-Length` from the collected body (since no
`Content-Length` came back), and send it to the client through `poll()`.

### 4.6 Concrete end-to-end example

Client sends:

```
POST /api/form.py?debug=1 HTTP/1.1
Host: localhost:8080
Content-Type: application/x-www-form-urlencoded
Content-Length: 12

name=ahmed
```

Server sets up this environment for the child (abridged):

```
REQUEST_METHOD=POST
SCRIPT_FILENAME=/var/www/api/form.py
QUERY_STRING=debug=1
CONTENT_LENGTH=12
CONTENT_TYPE=application/x-www-form-urlencoded
SERVER_PROTOCOL=HTTP/1.1
GATEWAY_INTERFACE=CGI/1.1
HTTP_HOST=localhost:8080
```

Server writes `name=ahmed\n` (12 bytes) into the child's stdin pipe, then closes
it (→ EOF). The script runs and writes to its stdout:

```
Content-Type: text/html

<html><body>Hi ahmed</body></html>
```

Script exits → pipe hits EOF → server has the full output. Server translates it
into a real response (adds status line + `Content-Length`):

```
HTTP/1.1 200 OK\r\n
Content-Type: text/html\r\n
Content-Length: 34\r\n
\r\n
<html><body>Hi ahmed</body></html>
```

### 4.7 Edge cases you must watch for with CGI

| # | Situation | Correct behavior |
|---|-----------|------------------|
| 1 | Chunked request body | Un-chunk before CGI; set `CONTENT_LENGTH` to assembled size |
| 2 | CGI returns no `Content-Length` | Read stdout until **EOF**; that marks the end (subject rule) |
| 3 | CGI hangs / infinite loop | Timeout → `kill` the child → `504`/`500`; never freeze the server |
| 4 | `execve` fails (bad interpreter/path) | Child exits non-zero → parent returns `500` |
| 5 | Script not found / not executable | `404`/`500` **before** forking (use `access`/`stat`) |
| 6 | Relative file access inside script | `chdir` to script's directory before `execve` |
| 7 | `php-cgi` refuses to run | It needs `REDIRECT_STATUS` (and proper `SCRIPT_FILENAME`) in the env |
| 8 | Large body / large output blocking a pipe | Non-blocking pipes under `poll()`; interleave writing input and reading output |
| 9 | Zombie processes accumulate | Always `waitpid`; don't leak children |
| 10 | Leaked file descriptors | `close` every pipe end you don't use, in both parent and child |
| 11 | CGI sets its own `Status:`/`Location:` | Honor it — map `Status:` to your status line; pass `Location:` through (redirects) |

---

<a name="5"></a>
## 5. Concept 5 — The full request→response workflow (text diagram)

This ties POST, DELETE, static files, and CGI into one picture. This is the
decision flow one connection travels through — all of it non-blocking, all reads
and writes gated by `poll()`.

```
                        ┌─────────────────────────────────────┐
                        │  poll() says a client FD is READABLE  │
                        └──────────────────┬────────────────────┘
                                           │ recv() available bytes
                                           ▼
                        ┌─────────────────────────────────────┐
                        │  Accumulate + parse the HTTP request  │
                        │  (request line, headers, then body)   │
                        │  Not complete yet? → return to poll,  │
                        │  keep this connection's state.        │
                        └──────────────────┬────────────────────┘
                                           │ request complete
                                           ▼
                        ┌─────────────────────────────────────┐
                        │  Match a server block + location      │
                        │  (host/port, longest path match)      │
                        └──────────────────┬────────────────────┘
                                           ▼
                        ┌─────────────────────────────────────┐
                        │  Is the method in allow_methods?      │───no──▶ 405 (+ Allow:)
                        └──────────────────┬────────────────────┘
                                           │ yes
                                           ▼
                        ┌─────────────────────────────────────┐
                        │  Is there a `return` (redirect)?      │───yes─▶ 3xx + Location:
                        └──────────────────┬────────────────────┘
                                           │ no
                                           ▼
              ┌────────────────────────────┼───────────────────────────────┐
              │                            │                                │
      is it a CGI ext?               method == DELETE                 method == POST
      (cgi_pass match)                     │                                │
              │                            ▼                                ▼
              ▼                 ┌───────────────────┐        ┌───────────────────────────┐
   ┌────────────────────┐      │ resolve path safe │        │ read body (len OR unchunk) │
   │ fork + pipe + dup2 │      │ (no `..` escape)  │        │ enforce client_max_body_size│
   │ execve interpreter │      │ exists? perms?    │        │  → 413 if exceeded         │
   │ write body→stdin   │      │  → 404 / 403      │        └──────────────┬────────────┘
   │ read stdout→EOF    │      │ unlink the file   │                       ▼
   │ poll() the pipes   │      │  → 204 (or 200)   │        ┌───────────────────────────┐
   │ waitpid + timeout  │      └─────────┬─────────┘        │ store to upload dir OR feed │
   │ parse CGI headers  │                │                  │ to CGI → 201 / 200 / 204    │
   │  → build response  │                │                  └──────────────┬────────────┘
   └─────────┬──────────┘                │                                 │
             │              (GET falls here: open file, or autoindex,      │
             │               or index file; 404 if missing)                │
             └────────────────────────────┼─────────────────────────────────┘
                                           ▼
                        ┌─────────────────────────────────────┐
                        │  Serialize: status line + headers +   │
                        │  CRLF blank line + body               │
                        │  On ANY error → default error page    │
                        │  with the accurate status code        │
                        └──────────────────┬────────────────────┘
                                           │ queue bytes for this FD
                                           ▼
                        ┌─────────────────────────────────────┐
                        │  poll() says the FD is WRITABLE →     │
                        │  send() the response (maybe in parts) │
                        │  then close or keep-alive             │
                        └─────────────────────────────────────┘
```

Read the diagram top to bottom once for the happy path, then trace each `──▶`
branch — those branches are your error/status-code map.

---

<a name="6"></a>
## 6. Self-check questions before you code

If you can answer these *out loud* to a peer (the subject literally grades your
ability to explain and modify your own code), you understand the concepts:

**POST**
- Why does POST need a body-reading state machine but GET doesn't?
- What are the two ways a client declares body length, and how do you detect
  each?
- Which status code means "a new file was created," and what header pairs with
  it?
- What happens when the body exceeds `client_max_body_size` — and *when* should
  you stop reading?

**DELETE**
- Why is DELETE's hard part path-resolution rather than body-reading?
- How do you stop `..` from deleting files outside the configured root?
- When would you answer `204` vs `200` vs `404` vs `403`?

**CGI**
- In one sentence: why does a C++ server need CGI at all?
- How does the request body get *into* the script, and how does the script's
  output get *back*? Name the exact mechanisms.
- If the CGI sends no `Content-Length`, how do you know its output is finished?
- Why must the CGI's pipes go through `poll()` and be non-blocking, like sockets?
- What does `chdir` before `execve` buy you, and what does `CONTENT_LENGTH` line
  up with?
- Which failures map to `502`, `504`, and `500`?

**Cross-cutting**
- Where is `fork` allowed, and why nowhere else?
- Why can't you inspect `errno` after a read/write to steer behavior, and what do
  you rely on instead?
- Name one way each method could make the server *hang*, and how your design
  prevents it.

---

*Built strictly from the webserv subject (v23.1). Verify every claim against the
subject and the HTTP/1.0 reference yourself, then test with `telnet` and compare
against NGINX — exactly as the subject instructs. Understand it before you build
it.*
