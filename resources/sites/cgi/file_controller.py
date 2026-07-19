import os
import re
import db_helper

def handle_get(params):
    files = db_helper.list_files()
    return {"status": "success", "files": files}

def parse_multipart(body_bytes, boundary):
    boundary_bytes = b"--" + boundary.encode('utf-8')
    parts = body_bytes.split(boundary_bytes)
    
    files = []
    for part in parts:
        # Strip trailing newlines or spaces if any
        if not part or part.strip() == b"" or part.strip() == b"--":
            continue
            
        if b"\r\n\r\n" in part:
            header_part, content = part.split(b"\r\n\r\n", 1)
        elif b"\n\n" in part:
            header_part, content = part.split(b"\n\n", 1)
        else:
            continue
            
        header_str = header_part.decode('utf-8', errors='ignore')
        
        # Strip trailing CRLF from content added by multipart boundary
        if content.endswith(b"\r\n"):
            content = content[:-2]
        elif content.endswith(b"\n"):
            content = content[:-1]
            
        if "Content-Disposition:" in header_str:
            filename_match = re.search(r'filename="([^"]+)"', header_str)
            if filename_match:
                filename = filename_match.group(1)
                files.append((filename, content))
    return files

def handle_post(content_type, body_bytes, query_params):
    # Method 1: Raw file upload via ?filename=name
    filename = query_params.get("filename")
    if filename:
        try:
            saved_name = db_helper.save_file(filename, body_bytes)
            return {"status": "success", "message": f"File {saved_name} uploaded successfully via raw body"}, 201
        except Exception as e:
            return {"status": "error", "message": f"Failed to save file: {str(e)}"}, 500
            
    # Method 2: Multipart Form-Data Upload
    if content_type and "multipart/form-data" in content_type:
        boundary_match = re.search(r'boundary=([^;\s]+)', content_type)
        if not boundary_match:
            return {"status": "error", "message": "Multipart boundary not found in Content-Type"}, 400
            
        boundary = boundary_match.group(1)
        # Strip outer quotes if browser wrapped boundary in quotes
        if boundary.startswith('"') and boundary.endswith('"'):
            boundary = boundary[1:-1]
            
        try:
            uploaded_files = parse_multipart(body_bytes, boundary)
            if not uploaded_files:
                return {"status": "error", "message": "No files found in multipart form-data"}, 400
                
            saved_files = []
            for fname, fcontent in uploaded_files:
                saved_name = db_helper.save_file(fname, fcontent)
                saved_files.append(saved_name)
                
            return {
                "status": "success", 
                "message": f"Successfully uploaded {len(saved_files)} file(s)", 
                "files": saved_files
            }, 201
        except Exception as e:
            return {"status": "error", "message": f"Multipart parse error: {str(e)}"}, 500
            
    return {"status": "error", "message": "Invalid POST request for file upload (must provide filename query parameter or multipart/form-data content type)"}, 400

def handle_delete(params):
    filename = params.get("file")
    if not filename:
        return {"status": "error", "message": "Filename ('file') is required in query params"}, 400
        
    success = db_helper.delete_file(filename)
    if success:
        return {"status": "success", "message": f"File {filename} deleted successfully"}
    else:
        return {"status": "error", "message": f"File {filename} not found"}, 404
