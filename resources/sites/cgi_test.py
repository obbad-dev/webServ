#!/usr/bin/env python3
"""
Minimal CGI test script.
If CGI works, this will return an HTML page showing:
  - The current server date and time
  - The REQUEST_METHOD used
  - All CGI environment variables your server passed
"""

import os
import sys
import datetime

# ---------- Build the HTML body ----------
now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
method = os.environ.get("REQUEST_METHOD", "(not set)")

env_rows = ""
for key in sorted(os.environ.keys()):
    env_rows += f"<tr><td><b>{key}</b></td><td>{os.environ[key]}</td></tr>\n"

html = f"""<!DOCTYPE html>
<html>
<head><title>CGI Test</title></head>
<body style="font-family: sans-serif; max-width: 800px; margin: 40px auto; padding: 0 20px;">
  <h1 style="color: green;">&#10004; CGI is Working!</h1>
  <p><b>Server Time:</b> {now}</p>
  <p><b>Request Method:</b> {method}</p>
  <hr>
  <h2>CGI Environment Variables</h2>
  <table border="1" cellpadding="6" cellspacing="0" style="border-collapse: collapse; width: 100%;">
    <tr style="background: #eee;"><th>Variable</th><th>Value</th></tr>
    {env_rows}
  </table>
</body>
</html>"""

# ---------- CGI output: headers + blank line + body ----------
sys.stdout.write("Status: 200 OK\r\n")
sys.stdout.write("Content-Type: text/html\r\n")
sys.stdout.write(f"Content-Length: {len(html)}\r\n")
sys.stdout.write("\r\n")
sys.stdout.write(html)
sys.stdout.flush()
