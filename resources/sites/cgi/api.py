#!/usr/bin/env python3
import sys
import os
import json
from urllib.parse import parse_qs

import task_controller
import file_controller

def get_env_diagnostics():
    keys = [
        "REQUEST_METHOD", "QUERY_STRING", "CONTENT_TYPE", "CONTENT_LENGTH",
        "SCRIPT_NAME", "SCRIPT_FILENAME", "SERVER_PROTOCOL", "GATEWAY_INTERFACE"
    ]
    return {k: os.environ.get(k, "") for k in keys}

def main():
    method = os.environ.get("REQUEST_METHOD", "GET").upper()
    query_string = os.environ.get("QUERY_STRING", "")
    content_type = os.environ.get("CONTENT_TYPE", "")
    content_length_str = os.environ.get("CONTENT_LENGTH", "0")
    
    # Parse query parameters
    params = {}
    if query_string:
        parsed = parse_qs(query_string)
        params = {k: v[0] for k, v in parsed.items()}
        
    # Read POST/PUT body if content_length is set
    body_bytes = b""
    if content_length_str:
        try:
            content_length = int(content_length_str)
            if content_length > 0:
                body_bytes = sys.stdin.buffer.read(content_length)
        except ValueError:
            pass

    resource = params.get("resource")
    
    # Routing
    result = None
    if resource == "tasks":
        if method == "GET":
            result = task_controller.handle_get(params)
        elif method == "POST":
            result = task_controller.handle_post(body_bytes.decode('utf-8', errors='ignore'))
        elif method == "DELETE":
            result = task_controller.handle_delete(params)
    elif resource == "files":
        if method == "GET":
            result = file_controller.handle_get(params)
        elif method == "POST":
            result = file_controller.handle_post(content_type, body_bytes, params)
        elif method == "DELETE":
            result = file_controller.handle_delete(params)
            
    # Default info response (diagnostics)
    if result is None:
        result = {
            "status": "success",
            "message": "Welcome to the WebServ Python CGI Multi-File Test Suite!",
            "diagnostics": {
                "cgi_env": get_env_diagnostics(),
                "query_parameters": params,
                "input_body_len": len(body_bytes)
            }
        }

    # Ensure result is a tuple (data, status_code)
    if isinstance(result, tuple):
        response_data, status = result
    else:
        response_data, status = result, 200

    # Inject environment diagnostics into response for testing
    if isinstance(response_data, dict):
        response_data["_env"] = get_env_diagnostics()

    # Output CGI headers and body
    # Since our webserver splits the response at the double newline,
    # we output dummy headers followed by the JSON body.
    response_body = json.dumps(response_data, indent=4)
    
    sys.stdout.write("Content-Type: application/json\r\n")
    sys.stdout.write(f"Status: {status}\r\n")
    sys.stdout.write(f"Content-Length: {len(response_body)}\r\n")
    sys.stdout.write("\r\n") # Double newline separator
    sys.stdout.write(response_body)
    sys.stdout.flush()

if __name__ == "__main__":
    main()
