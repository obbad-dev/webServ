import os
import json

DATA_FILE = "data.json"
UPLOADS_DIR = "uploads"

# Ensure uploads directory exists
if not os.path.exists(UPLOADS_DIR):
    os.makedirs(UPLOADS_DIR)

DEFAULT_TASKS = [
    {
        "id": 1,
        "title": "Welcome to Python CGI",
        "description": "This is a default task loaded from the CGI database module helper.",
        "priority": "High",
        "date": "2026-07-19"
    },
    {
        "id": 2,
        "title": "Test GET, POST and DELETE",
        "description": "Use the web interface to submit new tasks and delete them using CGI methods.",
        "priority": "Medium",
        "date": "2026-07-19"
    }
]

def load_tasks():
    if not os.path.exists(DATA_FILE):
        save_tasks(DEFAULT_TASKS)
        return DEFAULT_TASKS
    try:
        with open(DATA_FILE, "r") as f:
            return json.load(f)
    except Exception:
        return DEFAULT_TASKS

def save_tasks(tasks):
    with open(DATA_FILE, "w") as f:
        json.dump(tasks, f, indent=4)

def add_task(title, description, priority, date):
    tasks = load_tasks()
    new_id = max([t["id"] for t in tasks]) + 1 if tasks else 1
    new_task = {
        "id": new_id,
        "title": title,
        "description": description,
        "priority": priority,
        "date": date
    }
    tasks.append(new_task)
    save_tasks(tasks)
    return new_task

def delete_task(task_id):
    tasks = load_tasks()
    updated_tasks = [t for t in tasks if t["id"] != task_id]
    if len(updated_tasks) == len(tasks):
        return False
    save_tasks(updated_tasks)
    return True

def list_files():
    if not os.path.exists(UPLOADS_DIR):
        return []
    files = []
    for name in os.listdir(UPLOADS_DIR):
        path = os.path.join(UPLOADS_DIR, name)
        if os.path.isfile(path):
            files.append({
                "name": name,
                "size": os.path.getsize(path)
            })
    return files

def save_file(filename, content_bytes):
    # Sanitize filename
    safe_name = os.path.basename(filename)
    if not safe_name:
        raise ValueError("Invalid filename")
    
    dest_path = os.path.join(UPLOADS_DIR, safe_name)
    with open(dest_path, "wb") as f:
        f.write(content_bytes)
    return safe_name

def delete_file(filename):
    safe_name = os.path.basename(filename)
    dest_path = os.path.join(UPLOADS_DIR, safe_name)
    if os.path.exists(dest_path) and os.path.isfile(dest_path):
        os.remove(dest_path)
        return True
    return False
