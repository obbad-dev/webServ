# CGI Execution — Implementation Workflow

> A build-it-yourself checklist for the "execute CGI" method. This is the
> *workflow and decision points*, not a solution — you write the code.
> Grounded in the webserv subject: single `poll()`, non-blocking sockets/pipes,
> `fork` allowed only for CGI, EOF marks end of CGI output, `chdir` for relative
> paths, server must never hang or crash.

---

## 0. Read this before writing a line: the two hard problems

Everything in CGI comes down to two failure modes. Design against them first.

### Problem A — the size / memory problem (your 100MB question)

- **Gate it with `client_max_body_size` first.** If the body exceeds the route's
  limit → **413** and you never fork. A 100MB upload to a 3k-limit route dies
  here.
- If the body *is* legitimately large and allowed, you must **not** assume it
  fits in one buffer or one `write()`. Pipe buffers are ~64KB.
- HTTP "chunked" (`Transfer-Encoding: chunked`) is a *separate* concept: it's how
  the client framed the body on the wire. You **un-chunk** it into a plain body
  first, then set `CONTENT_LENGTH` to the assembled length. Un-chunking is about
  *framing*, not size.

### Problem B — the pipe deadlock (this is what actually hangs servers)

```
   YOU (parent)                          CGI (child)
   ────────────                          ───────────
   writing 100MB → stdin pipe            reading stdin,   producing output → stdout pipe
                 │                                                          │
   stdin pipe FULL (64KB) → write blocks │              stdout pipe FULL (64KB) → write blocks
                 │                                                          │
   you wait to write more ───────────────X──────────── child waits for you to drain stdout
                        BOTH STUCK FOREVER  → server hangs → grade 0
```

**Two ways to avoid it — pick one:**

| Approach | stdin (body → child) | stdout (child → you) | poll() needs |
|----------|----------------------|----------------------|--------------|
| **(A) Temp-file stdin** *(simpler, recommended)* | Write un-chunked body to a temp file, `dup2` its fd onto child stdin | pipe | poll **output pipe only** |
| **(B) Full pipe both ways** | pipe, write in pieces on `POLLOUT` | pipe, read on `POLLIN` | poll **both** simultaneously |

Approach (A) works because the subject exempts **regular files** from the
`poll()` requirement — the child reads its stdin file with normal blocking reads,
and you never risk the input-side deadlock. You only `poll()` the one output
pipe. Start with (A); it removes half the complexity and the 100MB case "just
works."

---

## 1. Inputs your `executeCGI` method needs

Before it runs, gather (from the router / request / config):

- The matched **interpreter** path (from `cgi_pass .py /usr/bin/python3`).
- The **script path** on disk (real filesystem path, from root + URL).
- The **request method**, **query string**, **headers**, and the **body**
  (already un-chunked, already checked against `client_max_body_size`).
- The **script's directory** (for `chdir`).
- The **server/connection info** (server name, port, protocol) for env vars.

Decide the method's return contract: it should ultimately produce either a
**complete HTTP response** or an **error status** (500/502/504/404) that your
response builder turns into an error page.

---

## 2. The workflow, phase by phase

### Phase 0 — Validate before forking (fail cheap)
1. Confirm the request is really CGI (extension matched a `cgi_pass`).
2. `access`/`stat` the **script file**: exists? readable? → else **404**.
3. Check the **interpreter** exists/executable → else **500**.
4. If method blocked for the route → **405** (done upstream, but re-assert).

> If anything fails here, return the error status **without** `fork`/`pipe`.
> Cheap failures should never spawn a process.

### Phase 1 — Prepare the body
1. Body is already un-chunked and within `client_max_body_size` (from POST
   handling). Know its exact **length** — that becomes `CONTENT_LENGTH`.
2. **Approach A:** write the body to a temp file now; keep its read fd.
   (Empty body for GET-style CGI → an empty temp file or `/dev/null` is fine.)

### Phase 2 — Build the environment array + argv
1. Build a `char *envp[]` of `"KEY=VALUE"` strings (you construct these by hand;
   there's no `setenv` in the allowed list — the array is the "set env" step).
2. Minimum set (see the reference table at the bottom): `REQUEST_METHOD`,
   `SCRIPT_FILENAME`, `SCRIPT_NAME`, `PATH_INFO`, `QUERY_STRING`,
   `CONTENT_LENGTH`, `CONTENT_TYPE`, `SERVER_PROTOCOL`, `SERVER_NAME`,
   `SERVER_PORT`, `GATEWAY_INTERFACE=CGI/1.1`, `REDIRECT_STATUS=200` (php-cgi
   needs it), plus each client header as `HTTP_*`.
3. Build `argv[] = { interpreter, script_path, NULL }`.
4. Remember C++98: build these in `std::vector<std::string>`, then produce the
   `char*` arrays right before `execve`. Keep the backing storage alive until
   after `execve`.

### Phase 3 — Set up the pipes
- **Approach A:** create **one** pipe for output (`out_pipe`). stdin comes from
  the temp file fd.
- **Approach B:** create **two** pipes (`in_pipe`, `out_pipe`).
- Set the ends you'll keep in the parent to **non-blocking**
  (macOS note from the subject: `fcntl` allowed only with `F_SETFL`,
  `O_NONBLOCK`, `FD_CLOEXEC`).

### Phase 4 — `fork()`
- `< 0` → cleanup + **500**.
- `== 0` → child (Phase 5).
- `> 0` → parent (Phase 6).

### Phase 5 — In the child (order matters)
```
(A) dup2(tempfile_read_fd, STDIN_FILENO)      // or in_pipe[0] for approach B
    dup2(out_pipe[write],  STDOUT_FILENO)
    close ALL other fds you inherited (both pipe ends, temp fd, listen socket…)
    chdir(script_directory)                    // subject: correct dir for relative paths
    execve(interpreter, argv, envp)
    // if execve returns, it FAILED:
    _exit(non-zero)                            // parent will read EOF + reap non-zero → 500/502
```
- **Close every unrelated fd** in the child (client sockets, the listen fd, the
  other pipe end). Leaked fds cause bugs and keep pipes from ever hitting EOF.
- Use `_exit`, not `return`, on `execve` failure.

### Phase 6 — In the parent: non-blocking I/O (the heart of it)
- Close the ends the child owns (`out_pipe[write]`, and `in_pipe[read]` for B).
- **Register the pipe fd(s) with your single global `poll()` loop** — do NOT
  spin in a private blocking loop here. The CGI's fds become just another
  connection your event loop services.
  - **Approach A:** watch `out_pipe[read]` for `POLLIN`. On each ready event,
    `read` a chunk and append to an output buffer. `read` returns `0` → **EOF →
    output complete**.
  - **Approach B:** additionally watch `in_pipe[write]` for `POLLOUT`; write the
    next slice of the body each time it's writable; when the whole body is sent,
    **close `in_pipe[write]`** — that close delivers EOF to the child's stdin so
    it stops reading and starts finishing.
- Never `read`/`write` a pipe without `poll()` saying it's ready. Never inspect
  `errno` to steer behavior — rely on readiness and on `read` returning 0 (EOF).

### Phase 7 — Reap the child + enforce a timeout
- Track a **start time / deadline** for the CGI when you fork.
- Poll your loop as usual; if the deadline passes and the child hasn't finished →
  `kill` it, `waitpid` it, return **504 Gateway Timeout** (or 500).
- On normal EOF, `waitpid` (non-blocking `WNOHANG` inside your loop, or after
  EOF) to get the exit status and avoid zombies.
  - Non-zero exit / signal → **502 Bad Gateway** (or 500).

### Phase 8 — Turn CGI output into an HTTP response
1. The collected stdout is **CGI-style**: some headers, a blank line
   (`\r\n\r\n`), then the body. Split on the first blank line.
2. Read the CGI headers:
   - `Status: 302 Found` → use it as your status line. No `Status:` → **200 OK**.
   - `Content-Type:` → pass through (default `text/plain` or `text/html` if
     missing — decide a rule).
   - `Location:` → pass through (redirects).
3. **The subject's EOF rule:** the CGI usually sends **no `Content-Length`**, so
   the body ended at the EOF you already detected. Now **you** add a real
   `Content-Length` (= number of body bytes you collected) so the browser is
   happy.
4. Serialize: `HTTP/1.1 <status>\r\n` + CGI headers + your `Content-Length` +
   `\r\n` + body. Queue it for the client fd, sent on `POLLOUT` like any
   response.

---

## 3. Sequence at a glance

```
validate (404/500 early)
   → prepare body  (un-chunked, size-checked; Approach A: write temp file)
   → build envp[] + argv[]
   → pipe(out)          [+ pipe(in) for Approach B]
   → fork
        child : dup2 stdin(tempfile/in), dup2 stdout(out), close rest,
                chdir(scriptdir), execve → _exit on fail
        parent: close child ends
                register pipe fd(s) in global poll()
                POLLIN  → read stdout until EOF (=body done)
                POLLOUT → (B) feed body, then close to send EOF
                deadline passed → kill → 504
   → waitpid (status: !=0 → 502/500)
   → split CGI headers/body, add status line + Content-Length
   → hand response to the writer (sent on client POLLOUT)
```

---

## 4. Edge cases checklist

| # | Case | Handling |
|---|------|----------|
| 1 | Body > `client_max_body_size` (your 100MB) | **413**, before fork |
| 2 | Chunked request | un-chunk → set `CONTENT_LENGTH` to assembled size |
| 3 | Large allowed body | Approach A (temp file) or Approach B (write on `POLLOUT`) — never one big `write` |
| 4 | Pipe deadlock | poll output (A) / both (B); drain stdout while feeding stdin |
| 5 | CGI sends no `Content-Length` | read stdout to **EOF**; you compute Content-Length |
| 6 | CGI hangs / infinite loop | deadline → `kill` → **504** |
| 7 | `execve` fails | child `_exit(non-zero)` → parent → **500** |
| 8 | Script missing / not executable | **404/500** before fork (`access`/`stat`) |
| 9 | Zombie children | always `waitpid` |
| 10 | Leaked fds (pipe never EOFs) | child closes ALL inherited fds; parent closes child ends |
| 11 | Relative file access in script | `chdir` to script dir before `execve` |
| 12 | php-cgi won't run | set `REDIRECT_STATUS=200` and a correct `SCRIPT_FILENAME` |
| 13 | CGI sets `Status:` / `Location:` | honor them in the final response |
| 14 | Client disconnects mid-CGI | detect, `kill` child, clean up — don't leak the process |
| 15 | Empty body (GET-style CGI) | empty temp file / `/dev/null`; `CONTENT_LENGTH=0` |

---

## 5. Environment variable reference

```
REQUEST_METHOD       GET | POST | DELETE
SCRIPT_NAME          /api/form.py            (URL path of the script)
SCRIPT_FILENAME      /var/www/api/form.py    (real path; php-cgi relies on it)
PATH_INFO            /extra                  (path after the script, if any)
QUERY_STRING         a=1&b=2                 (after '?' in URL; empty if none)
CONTENT_LENGTH       27                      (bytes of body on stdin)
CONTENT_TYPE         multipart/form-data; boundary=…   (verbatim — script parses it)
SERVER_PROTOCOL      HTTP/1.1
SERVER_NAME          localhost
SERVER_PORT          8080
GATEWAY_INTERFACE    CGI/1.1
REDIRECT_STATUS      200                     (php-cgi refuses to start without it)
HTTP_<HEADER>        one per client header: "User-Agent" → HTTP_USER_AGENT
                     (uppercase, '-' → '_', prefix HTTP_); e.g. HTTP_COOKIE for cookies
```

> Note for POST-to-CGI: pass the body **raw** (only un-chunked). If it's
> `multipart/form-data`, do **not** parse it — copy `CONTENT_TYPE` verbatim
> (boundary included) and the script parses the parts itself. You only parse
> multipart when *your server* stores the upload directly.

---

## 7. Wiring Approach B into YOUR code (epoll + FdManager)

> This section is specific to the code in `~/Desktop/webServ`. Symbols
> referenced: `FdManager`, `ServerSide::communication_part()`,
> `add_fd_to_epoll` / `change_epoll_event` / `remove_from_epoll`,
> `HttpResponseBuilder::build()` / `executeCGI()`, `manager.epollFd`,
> `request.getBodyContent()`, `response.serializeResponse()`,
> `response.init_bytes_var()`, `send_response()`.

### 7.0 The mindset shift

Your `build()` is **synchronous** — the loop flips to `EPOLLOUT` right after it
returns. CGI is **asynchronous**: `executeCGI` must **start** the child + pipes,
register the pipe fds in your one epoll, and **return without a finished
response**. The response is assembled *later*, when the stdout pipe reaches EOF.
Treat the CGI pipe fds as **new first-class fds in the same epoll loop**, exactly
like client sockets.

### 7.1 Two must-do guards first (or the server dies)

1. **`signal(SIGPIPE, SIG_IGN)` once in `setup()`.** Writing to the stdin pipe
   after the child closed its read end raises `SIGPIPE` → default kills the
   server. `signal` is in the allowed list.
2. **Guard the event dispatch.** In `communication_part()`, `fds.find(fd)` then
   `it->second.type` crashes for pipe fds (not in `fds`). Add a CGI branch
   **before** the SERVER/CLIENT branch (see 7.4), and never deref `fds.end()`.
3. **Close-on-exec / close-in-child.** The forked CGI child inherits every open
   fd (all client sockets, listen sockets, the epoll fd, other CGI pipes). Either
   set `FD_CLOEXEC` (allowed) on those long-lived fds when you create them, or
   `close()` them all in the child before `execve`. Otherwise pipes never reach
   EOF and you leak fds.

### 7.2 State to add (put it in `FdManager`, or a `CgiState` it owns)

```
pid_t   cgiPid;
int     cgiIn;        // parent's WRITE end → child stdin   (-1 when closed)
int     cgiOut;       // parent's READ  end ← child stdout   (-1 when closed)
size_t  bodyOffset;   // how much of request body已 written
string  cgiOutBuf;    // accumulated CGI stdout
bool    cgiRunning;
time_t  cgiStart;     // for the CGI timeout
```

And in `ServerSide`, a way to route a pipe event back to its client:

```
map<int,int> cgiPipeToClient;   // pipe fd  ->  client fd
```

> Note: `FdManager` doesn't currently store its own fd, and `executeCGI` can't
> see `cgiPipeToClient`. Cleanest fix given your structure: **`executeCGI` only
> forks + makes pipes + fills the `CgiState` in `manager`, then returns a status
> code** (e.g. `CGI_STARTED`). The **loop** (which knows the client fd `it->first`
> and owns the maps) does the epoll registration + map bookkeeping.

### 7.3 Refactor `build()` / `executeCGI()` — start, don't finish

Make `build()` return a status so the loop knows what to do next:

```
enum BuildResult { RESPONSE_READY, CGI_STARTED };
```

`executeCGI(manager, physicalPath)` should:
1. Body is already assembled in `request.getBodyContent()` (your parser handles
   CONTENT_LENGTH and CHUNKED). Know its length → `CONTENT_LENGTH`.
2. Build `envp[]` (section 5) and `argv[] = { interpreter, physicalPath, NULL }`.
   Interpreter comes from `location->getCgiPass()[ext]`.
3. `pipe(inPipe); pipe(outPipe);`
4. Parent ends (`inPipe[1]`, `outPipe[0]`) → non-blocking (+ `FD_CLOEXEC`).
5. `fork()`:
   - **child:** `dup2(inPipe[0], 0)`, `dup2(outPipe[1], 1)`, close all 4 pipe fds
     (and inherited fds if not CLOEXEC), `chdir(dirname(physicalPath))`,
     `execve(interpreter, argv, envp)`, `_exit(1)` if it returns.
   - **parent:** `close(inPipe[0])`, `close(outPipe[1])`; store
     `cgiIn=inPipe[1]`, `cgiOut=outPipe[0]`, `cgiPid`, `bodyOffset=0`,
     `cgiStart=time(NULL)`, `cgiRunning=true`.
6. **If the body is empty** (e.g. GET-CGI): `close(cgiIn); cgiIn=-1;` right away
   so you never wait for `EPOLLOUT`.
7. Return `CGI_STARTED`.

In `communication_part()` where you currently call `build()`:

```
if (build(manager) == CGI_STARTED) {
    if (manager.cgiOut != -1) add_fd_to_epoll(epollFd, manager.cgiOut, EPOLLIN);
    if (manager.cgiIn  != -1) add_fd_to_epoll(epollFd, manager.cgiIn,  EPOLLOUT);
    cgiPipeToClient[manager.cgiOut] = clientFd;      // and cgiIn if open
    change_epoll_event(epollFd, clientFd, 0);        // park client; nothing to do yet
} else {
    change_epoll_event(epollFd, clientFd, EPOLLOUT); // your current behaviour
}
```

Parking the client with events `0` stops the loop from re-parsing/re-sending on
it mid-CGI. (Keep a `cgiRunning` flag so if the client fd still fires `EPOLLHUP`,
you treat it as disconnect → kill child + cleanup.)

### 7.4 Add the CGI branch in the event loop

At the top of the per-event body, before the `type == SERVER` check:

```
if (cgiPipeToClient.count(event.data.fd)) {
    handleCgiIo(event.data.fd, event.events);   // 7.5
    continue;
}
```

### 7.5 `handleCgiIo(fd, events)` — the non-blocking pump

Look up the client via `cgiPipeToClient[fd]`, get its `FdManager`. **One** syscall
per event (level-triggered epoll guarantees readiness; never loop-until-EAGAIN,
never read `errno`).

**If `fd == cgiOut` (readable / HUP):**
```
n = read(cgiOut, buf, BUFSZ);
n > 0  → cgiOutBuf.append(buf, n);            // keep going, epoll will refire
n == 0 → EOF: remove_from_epoll + close(cgiOut); cgiOut=-1;
         cgiPipeToClient.erase(fd);
         finalizeCgi(manager, clientFd);       // 7.6
n < 0  → error: kill child, finalize as 502/500
```

**If `fd == cgiIn` (writable):**
```
const string& body = manager.request.getBodyContent();
n = write(cgiIn, body.data()+bodyOffset, min(CHUNK, body.size()-bodyOffset));
n > 0  → bodyOffset += n;
         if (bodyOffset == body.size()) {      // whole body sent
             remove_from_epoll + close(cgiIn); cgiIn=-1;   // close = EOF to child stdin
             cgiPipeToClient.erase(fd);
         }
n <= 0 → child likely closed its stdin early (SIGPIPE ignored, so write just
         fails): close(cgiIn); cgiIn=-1; erase map — treat input as done.
```

This is exactly what makes the **100MB body safe**: you write at most `CHUNK`
bytes each time epoll says `cgiIn` is writable, and you drain `cgiOut` in the
same loop — so neither pipe can fill and deadlock.

### 7.6 `finalizeCgi(manager, clientFd)` — build the real response on EOF

```
waitpid(cgiPid, &status, 0);                  // reap; avoid zombie
// (non-zero / signaled child → 502/500 instead of the below)

split cgiOutBuf at first "\r\n\r\n"  →  cgiHeaders + cgiBody
parse cgiHeaders:  Status: → status line (default 200 OK)
                   Content-Type: → pass through
                   Location: → pass through
response.setStatusCode(...); response.setResponseHeader("Content-Type", ...);
response.setResponseHeader("Content-Length", intToString(cgiBody.size()));  // EOF rule: you compute it
response.setResponseBody(cgiBody);
response.serializeResponse("HTTP/1.1");
response.init_bytes_var();                     // reset bytesSent!
change_epoll_event(epollFd, clientFd, EPOLLOUT);   // now the client sends
manager.cgiRunning = false;
```

From here your existing `EPOLLOUT` → `send_response()` path ships it unchanged.

### 7.7 Timeout + cleanup (resilience is graded)

- Your current timeout loop only disconnects idle **CLIENT**s. Add: if
  `cgiRunning && now - cgiStart > CGI_TIMEOUT` → `kill(cgiPid, SIGKILL)`, reap,
  build **504**, flip client to `EPOLLOUT`, close pipes, erase map entries. Also
  make sure a parked client (events 0) mid-CGI isn't wrongly killed by the plain
  `TIMEOUT` check.
- If the **client disconnects during CGI** (its fd HUPs): `kill` the child, close
  both pipes, erase both map entries, then drop the client.
- Every exit path closes `cgiIn`/`cgiOut` if still open and erases their
  `cgiPipeToClient` entries. Leaked pipe fds = pipes that never EOF = hangs.

### 7.8 Order-of-work checklist for your code

1. `signal(SIGPIPE, SIG_IGN)` in `setup()`.
2. `FD_CLOEXEC` on listen/epoll/client fds (or close-in-child).
3. Add `CgiState` fields to `FdManager` + `cgiPipeToClient` to `ServerSide`.
4. Change `build()`/`executeCGI` to return `CGI_STARTED` and start the child.
5. Register pipe fds + park client fd in the loop after `build()`.
6. Add the CGI dispatch branch + `handleCgiIo` in `communication_part()`.
7. Add `finalizeCgi` (headers/body split, Content-Length, flip to EPOLLOUT).
8. Add CGI timeout + disconnect cleanup.
9. **Fix the `errno`-after-`send`/`read` usage** (`send_response`, accept loop) —
   forbidden by the subject; do one op per readiness event instead.

---

## 6. Self-check before you call it done

- Can you explain why writing 100MB to the stdin pipe in one call would hang the
  whole server, and which of Approach A/B you chose to prevent it?
- Where exactly does the CGI learn the body length, and where does it learn the
  body *ended*?
- Which fd close produces the EOF the child sees on stdin? Which EOF tells *you*
  the output is done?
- Name the status you return for: script missing, execve fail, non-zero exit,
  timeout, body too large.
- Every pipe read/write goes through the one `poll()` loop — no private blocking
  loop hiding in your CGI method?
```
