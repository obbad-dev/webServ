#!/usr/bin/env python3
"""
CGI API script for the Python CGI Dashboard & Multi-File Test Suite.

Handles:
  - GET    /cgi/api.py?resource=tasks          → list all tasks
  - POST   /cgi/api.py?resource=tasks          → create a task (JSON body)
  - DELETE /cgi/api.py?resource=tasks&id=<id>   → delete a task by id
  - GET    /cgi/api.py?resource=files           → list uploaded files
  - POST   /cgi/api.py?resource=files           → upload a file (multipart)
  - DELETE /cgi/api.py?resource=files&file=<n>  → delete an uploaded file

All state is persisted to a JSON file (tasks) and a local uploads/ directory.
"""

import os
import sys
import json
import time

# ---------------------------------------------------------------------------
# Paths – resolved relative to this script's own location so that the server's
# working directory doesn't matter.
# ---------------------------------------------------------------------------
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_FILE = os.path.join(SCRIPT_DIR, "tasks.json")
UPLOAD_DIR = os.path.join(SCRIPT_DIR, "uploads")

# Ensure the uploads directory exists
os.makedirs(UPLOAD_DIR, exist_ok=True)


# ===========================================================================
#  Helpers
# ===========================================================================

def get_env_snapshot():
    """Return a dict of the CGI environment variables the frontend expects."""
    keys = [
        "REQUEST_METHOD", "QUERY_STRING", "CONTENT_TYPE",
        "CONTENT_LENGTH", "SCRIPT_NAME", "SCRIPT_FILENAME",
        "SERVER_PROTOCOL", "GATEWAY_INTERFACE",
    ]
    return {k: os.environ.get(k, "") for k in keys}


def respond(status_code, status_text, body_dict):
    """Write a CGI response (headers + JSON body) to stdout."""
    body_dict["_env"] = get_env_snapshot()
    payload = json.dumps(body_dict)
    # CGI/1.1 output: headers, blank line, body
    sys.stdout.write(f"Status: {status_code} {status_text}\r\n")
    sys.stdout.write("Content-Type: application/json\r\n")
    sys.stdout.write(f"Content-Length: {len(payload)}\r\n")
    sys.stdout.write("\r\n")
    sys.stdout.write(payload)
    sys.stdout.flush()


def read_body():
    """Read the raw request body from stdin based on CONTENT_LENGTH."""
    length = int(os.environ.get("CONTENT_LENGTH", 0) or 0)
    if length <= 0:
        return b""
    return sys.stdin.buffer.read(length)


def parse_query_string(qs):
    """Minimal query-string parser (no external deps)."""
    params = {}
    if not qs:
        return params
    for part in qs.split("&"):
        if "=" in part:
            key, value = part.split("=", 1)
            params[_url_decode(key)] = _url_decode(value)
        else:
            params[_url_decode(part)] = ""
    return params


def _url_decode(s):
    """Decode percent-encoded characters."""
    import re
    def _replace(m):
        return chr(int(m.group(1), 16))
    s = s.replace("+", " ")
    return re.sub(r"%([0-9A-Fa-f]{2})", _replace, s)


# ===========================================================================
#  Task storage  (simple JSON file)
# ===========================================================================

def load_tasks():
    if not os.path.exists(DATA_FILE):
        return []
    try:
        with open(DATA_FILE, "r") as f:
            return json.load(f)
    except (json.JSONDecodeError, IOError):
        return []


def save_tasks(tasks):
    with open(DATA_FILE, "w") as f:
        json.dump(tasks, f, indent=2)


def next_task_id(tasks):
    if not tasks:
        return 1
    return max(t.get("id", 0) for t in tasks) + 1


# ===========================================================================
#  Multipart parser  (minimal, no external deps)
# ===========================================================================

def parse_multipart(body, content_type):
    """
    Very small multipart/form-data parser.
    Returns a list of dicts: {name, filename, content_type, data}
    """
    # Extract boundary from Content-Type header
    boundary = None
    for part in content_type.split(";"):
        part = part.strip()
        if part.startswith("boundary="):
            boundary = part[len("boundary="):]
            # Strip optional quotes
            boundary = boundary.strip('"')
            break
    if not boundary:
        return []

    delimiter = ("--" + boundary).encode()
    end_delimiter = ("--" + boundary + "--").encode()

    parts = []
    segments = body.split(delimiter)

    for segment in segments:
        if not segment or segment.strip() == b"" or segment.strip() == b"--":
            continue
        if segment.startswith(end_delimiter.replace(delimiter, b"")):
            continue

        # Remove leading \r\n
        if segment.startswith(b"\r\n"):
            segment = segment[2:]

        # Split headers from body
        header_end = segment.find(b"\r\n\r\n")
        if header_end == -1:
            continue

        header_block = segment[:header_end].decode("utf-8", errors="replace")
        data = segment[header_end + 4:]

        # Remove trailing \r\n
        if data.endswith(b"\r\n"):
            data = data[:-2]

        # Parse headers
        name = None
        filename = None
        ct = "application/octet-stream"
        for line in header_block.split("\r\n"):
            lower = line.lower()
            if lower.startswith("content-disposition:"):
                # Extract name
                if 'name="' in line:
                    start = line.index('name="') + 6
                    end = line.index('"', start)
                    name = line[start:end]
                # Extract filename
                if 'filename="' in line:
                    start = line.index('filename="') + 10
                    end = line.index('"', start)
                    filename = line[start:end]
            elif lower.startswith("content-type:"):
                ct = line.split(":", 1)[1].strip()

        if name:
            parts.append({
                "name": name,
                "filename": filename,
                "content_type": ct,
                "data": data,
            })

    return parts


# ===========================================================================
#  Route handlers
# ===========================================================================

def handle_get_tasks():
    tasks = load_tasks()
    respond(200, "OK", {"status": "success", "tasks": tasks})


def handle_post_task(body):
    try:
        payload = json.loads(body)
    except (json.JSONDecodeError, ValueError):
        respond(400, "Bad Request", {"status": "error", "message": "Invalid JSON"})
        return

    title = payload.get("title", "").strip()
    description = payload.get("description", "").strip()
    priority = payload.get("priority", "Medium")
    date = payload.get("date", "")

    if not title or not description:
        respond(400, "Bad Request", {
            "status": "error",
            "message": "Title and description are required"
        })
        return

    tasks = load_tasks()
    task = {
        "id": next_task_id(tasks),
        "title": title,
        "description": description,
        "priority": priority,
        "date": date,
    }
    tasks.append(task)
    save_tasks(tasks)
    respond(201, "Created", {"status": "success", "task": task})


def handle_delete_task(params):
    task_id = params.get("id")
    if not task_id:
        respond(400, "Bad Request", {
            "status": "error",
            "message": "Missing 'id' parameter"
        })
        return

    try:
        task_id = int(task_id)
    except ValueError:
        respond(400, "Bad Request", {
            "status": "error",
            "message": "Invalid task id"
        })
        return

    tasks = load_tasks()
    new_tasks = [t for t in tasks if t.get("id") != task_id]

    if len(new_tasks) == len(tasks):
        respond(404, "Not Found", {
            "status": "error",
            "message": f"Task {task_id} not found"
        })
        return

    save_tasks(new_tasks)
    respond(200, "OK", {"status": "success", "message": f"Task {task_id} deleted"})


def handle_get_files():
    files = []
    if os.path.isdir(UPLOAD_DIR):
        for name in sorted(os.listdir(UPLOAD_DIR)):
            filepath = os.path.join(UPLOAD_DIR, name)
            if os.path.isfile(filepath):
                files.append({
                    "name": name,
                    "size": os.path.getsize(filepath),
                })
    respond(200, "OK", {"status": "success", "files": files})


def handle_post_file(body, content_type):
    parts = parse_multipart(body, content_type)

    file_part = None
    for p in parts:
        if p["name"] == "file" and p.get("filename"):
            file_part = p
            break

    if not file_part:
        respond(400, "Bad Request", {
            "status": "error",
            "message": "No file found in upload"
        })
        return

    # Sanitise the filename – keep only the basename
    filename = os.path.basename(file_part["filename"])
    if not filename:
        filename = f"upload_{int(time.time())}"

    dest = os.path.join(UPLOAD_DIR, filename)
    with open(dest, "wb") as f:
        f.write(file_part["data"])

    respond(201, "Created", {
        "status": "success",
        "message": f"File '{filename}' uploaded",
        "file": {
            "name": filename,
            "size": len(file_part["data"]),
        }
    })


def handle_delete_file(params):
    filename = params.get("file", "")
    if not filename:
        respond(400, "Bad Request", {
            "status": "error",
            "message": "Missing 'file' parameter"
        })
        return

    # Prevent path traversal
    filename = os.path.basename(filename)
    filepath = os.path.join(UPLOAD_DIR, filename)

    if not os.path.isfile(filepath):
        respond(404, "Not Found", {
            "status": "error",
            "message": f"File '{filename}' not found"
        })
        return

    os.remove(filepath)
    respond(200, "OK", {
        "status": "success",
        "message": f"File '{filename}' deleted"
    })


# ===========================================================================
#  Main – CGI entry point
# ===========================================================================

def main():
    method = os.environ.get("REQUEST_METHOD", "GET").upper()
    query_string = os.environ.get("QUERY_STRING", "")
    content_type = os.environ.get("CONTENT_TYPE", "")
    params = parse_query_string(query_string)
    resource = params.get("resource", "")

    if resource == "tasks":
        if method == "GET":
            handle_get_tasks()
        elif method == "POST":
            body = read_body()
            handle_post_task(body)
        elif method == "DELETE":
            handle_delete_task(params)
        else:
            respond(405, "Method Not Allowed", {
                "status": "error",
                "message": f"Method {method} not allowed on tasks"
            })

    elif resource == "files":
        if method == "GET":
            handle_get_files()
        elif method == "POST":
            body = read_body()
            handle_post_file(body, content_type)
        elif method == "DELETE":
            handle_delete_file(params)
        else:
            respond(405, "Method Not Allowed", {
                "status": "error",
                "message": f"Method {method} not allowed on files"
            })

    else:
        respond(400, "Bad Request", {
            "status": "error",
            "message": f"Unknown resource: '{resource}'"
        })


if __name__ == "__main__":
    main()
