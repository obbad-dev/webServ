# QA Test Plan for webServ config parser

This folder contains manual QA fixtures for the C++98 parser in this workspace.

How to use:
1. Build the binary with `make`.
2. Run `./webServ tests/qa/valid/<file>.conf` for expected success cases.
3. Run `./webServ tests/qa/invalid/<file>.conf` for expected failure cases.
4. Compare the output to the notes below.

Notes:
- The parser currently accepts `listen`, `root`, `index`, `client_max_body_size`, `error_page`, `server_name`, and `location` directives.
- `autoindex` is currently validated inside `location` blocks; it is not accepted at the server level by this implementation.
- Location blocks inherit `root` and `index` from the server when those are not explicitly set.
- The parser prints a debug dump on success.
