# QA Test Plan for the configuration parser

This plan is based on the current implementation in this repository and has been verified with the built binary in this workspace.

## 1. How to run the plan

1. Build the parser with `make`.
2. Run each config with `./webServ tests/qa/valid/<name>.conf` for success cases.
3. Run each config with `./webServ tests/qa/invalid/<name>.conf` for failure cases.
4. Compare the output to the expected result listed below.

## 2. Valid configuration tests

| Case | File | What to verify | Expected result |
| --- | --- | --- | --- |
| Basic server | [tests/qa/valid/01_basic_server.conf](tests/qa/valid/01_basic_server.conf) | Minimal valid server with `listen`, `root`, `index`, `client_max_body_size`, `error_page`. | Exit code 0. The debug dump should show `port: 8080`, `Root: /var/www`, `client max body size: 2097152B`, `error_page: 404 500 /errors/50x.html`. |
| Multiple servers | [tests/qa/valid/02_multiple_servers.conf](tests/qa/valid/02_multiple_servers.conf) | Two separate `server {}` blocks. | Exit code 0. The dump should contain two server blocks, one on `8080` and one on `127.0.0.1:8081`. |
| Location inheritance | [tests/qa/valid/03_location_inheritance.conf](tests/qa/valid/03_location_inheritance.conf) | Location inherits `root` and `index` from the server when the location does not override them. | Exit code 0. The location `/assets` should show `root: /srv/app` and `index: index.html index.php default.html` if no explicit override exists. |
| Whitespace/comments | [tests/qa/valid/04_whitespace_and_comments.conf](tests/qa/valid/04_whitespace_and_comments.conf) | Extra spaces, comments, and mixed formatting. | Exit code 0. The parser should still accept `listen 127.0.0.1:8082;`, `root /var/www/site;`, and multiple `error_page` codes. |
| Inheritance + overrides | [tests/qa/valid/05_inheritance_and_overrides.conf](tests/qa/valid/05_inheritance_and_overrides.conf) | Location with explicit overrides and one location that inherits `root/index`. | Exit code 0. The `/static` location should show its own `root: /srv/static-root` and `index: home.static.html`, while `/` inherits the server root/index. |
| Nginx-style sample | [tests/qa/valid/06_nginx_style_sample.conf](tests/qa/valid/06_nginx_style_sample.conf) | Typical Nginx-style layout with multiple `error_page` lines and a location block. | Exit code 0. The dump should show `client max body size: 10485760B` and `error_page: 404 500 502 503 /50x.html`. |

## 3. Invalid configuration tests

| Case | File | What to test | Expected error message |
| --- | --- | --- | --- |
| Missing semicolon | [tests/qa/invalid/01_missing_semicolon.conf](tests/qa/invalid/01_missing_semicolon.conf) | Missing `;` after `listen`. | `Syntax error: too many values in directive listen` |
| Unsupported directive | [tests/qa/invalid/02_unsupported_directive.conf](tests/qa/invalid/02_unsupported_directive.conf) | Unknown key `foo`. | `Unsupported directive: 'foo'.` |
| Invalid listen port | [tests/qa/invalid/03_invalid_listen.conf](tests/qa/invalid/03_invalid_listen.conf) | Out-of-range port value. | `Port must be between 1 and 65535 for the 'listen' directive: 99999` |
| Invalid autoindex value | [tests/qa/invalid/04_invalid_autoindex.conf](tests/qa/invalid/04_invalid_autoindex.conf) | Bad `autoindex` value in a location. | `Invalid value for autoindex: 'maybe'. Valid values are 'on' or 'off'.` |
| Invalid `error_page` status | [tests/qa/invalid/05_invalid_error_page.conf](tests/qa/invalid/05_invalid_error_page.conf) | Status code outside 400-500 range. | `number of error page must be in range 400 - 500` |
| Duplicate `root` | [tests/qa/invalid/06_duplicate_root.conf](tests/qa/invalid/06_duplicate_root.conf) | Repeated `root` in the same server block. | `duplicate root directive in server block` |
| Missing required `root` | [tests/qa/invalid/07_missing_root.conf](tests/qa/invalid/07_missing_root.conf) | Server without `root`. | `Config file must contain directives 'listen' and 'root'.` |
| Duplicate `location` path | [tests/qa/invalid/08_duplicate_location.conf](tests/qa/invalid/08_duplicate_location.conf) | Two `location /x {}` blocks. | `duplicate location block for path: /x` |
| Missing location brace | [tests/qa/invalid/09_location_missing_brace.conf](tests/qa/invalid/09_location_missing_brace.conf) | `location /x` without `{`. | `directive "location" has no opening "{"` |
| Invalid `client_max_body_size` unit | [tests/qa/invalid/10_invalid_client_max_body_size.conf](tests/qa/invalid/10_invalid_client_max_body_size.conf) | Unsupported suffix `x`. | `Unsupported unit: 10x in client_max_body_size directive` |
| Invalid listen IP | [tests/qa/invalid/11_invalid_listen_ip.conf](tests/qa/invalid/11_invalid_listen_ip.conf) | Bad IPv4 octets. | `The 'listen' directive is not valid: invalid IP address -> 999.999.999.999` |
| `error_page` missing URI | [tests/qa/invalid/12_invalid_error_page_missing_uri.conf](tests/qa/invalid/12_invalid_error_page_missing_uri.conf) | `error_page 404;` with no URI. | `error_page: missing status code or URI` |
| Empty `index` value | [tests/qa/invalid/13_invalid_index_value.conf](tests/qa/invalid/13_invalid_index_value.conf) | `index ;`. | `index: no index files specified.` |
| Duplicate `listen` value | [tests/qa/invalid/14_duplicate_listen.conf](tests/qa/invalid/14_duplicate_listen.conf) | Same port repeated. | `Listen directive duplicated: 8080` |
| Unclosed brace | [tests/qa/invalid/15_unclosed_brace.conf](tests/qa/invalid/15_unclosed_brace.conf) | Missing final `}`. | `Error: Unclosed brace '{'` |
| Unmatched closing brace | [tests/qa/invalid/16_unmatched_closing_brace.conf](tests/qa/invalid/16_unmatched_closing_brace.conf) | Extra `}` at the end. | `Error: Unmatched closing brace '}'` |

## 4. Nginx comparison tests

Use these checks to compare the parser behavior with common Nginx expectations:

1. Multiple `error_page` directives should accumulate status codes and keep the same URI.
   - Verified by [tests/qa/valid/06_nginx_style_sample.conf](tests/qa/valid/06_nginx_style_sample.conf).
   - Expected debug output should contain `error_page: 404 500 502 503 /50x.html`.

2. Location blocks should inherit server-level `root` and `index` unless they override them.
   - Verified by [tests/qa/valid/05_inheritance_and_overrides.conf](tests/qa/valid/05_inheritance_and_overrides.conf).
   - Expected behavior: `/` inherits `root /srv/server-root` and `index index.server.html`, while `/static` overrides both with its own values.

3. Whitespace and comments should not change the parse result.
   - Verified by [tests/qa/valid/04_whitespace_and_comments.conf](tests/qa/valid/04_whitespace_and_comments.conf).
   - Expected behavior: Same semantic result as a compact config.

4. Invalid values should fail early with a clear message, matching the parser’s current validation style.
   - Verified by the invalid fixtures above.
   - Expected behavior: `listen`, `client_max_body_size`, `autoindex`, and `error_page` all produce explicit validation errors.

## 5. Pass/fail checklist

Mark each case as:
- PASS: the process exit code matches the expected result and the printed message contains the expected text.
- FAIL: exit code or message differs from the expected result.

This gives you a manual QA checklist you can run repeatedly while you refine the parser.
