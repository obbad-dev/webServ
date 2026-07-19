import json
import db_helper

def handle_get(params):
    tasks = db_helper.load_tasks()
    
    # Filter by ID if specified
    task_id_str = params.get("id")
    if task_id_str:
        try:
            task_id = int(task_id_str)
            for t in tasks:
                if t["id"] == task_id:
                    return {"status": "success", "task": t}
            return {"status": "error", "message": "Task not found"}, 404
        except ValueError:
            return {"status": "error", "message": "Invalid Task ID format"}, 400
            
    # Filter by Priority if specified
    priority = params.get("priority")
    if priority:
        filtered = [t for t in tasks if t["priority"].lower() == priority.lower()]
        return {"status": "success", "tasks": filtered}
        
    return {"status": "success", "tasks": tasks}

def handle_post(body_str):
    try:
        data = json.loads(body_str)
    except Exception as e:
        return {"status": "error", "message": f"Malformed JSON: {str(e)}"}, 400
        
    title = data.get("title", "").strip()
    description = data.get("description", "").strip()
    priority = data.get("priority", "Medium").strip()
    date = data.get("date", "").strip()
    
    if not title:
        return {"status": "error", "message": "Title is required"}, 400
        
    new_task = db_helper.add_task(title, description, priority, date)
    return {"status": "success", "message": "Task created successfully", "task": new_task}, 201

def handle_delete(params):
    task_id_str = params.get("id")
    if not task_id_str:
        return {"status": "error", "message": "Task ID ('id') is required in query params"}, 400
        
    try:
        task_id = int(task_id_str)
    except ValueError:
        return {"status": "error", "message": "Invalid Task ID format"}, 400
        
    success = db_helper.delete_task(task_id)
    if success:
        return {"status": "success", "message": f"Task {task_id} deleted successfully"}
    else:
        return {"status": "error", "message": f"Task {task_id} not found"}, 404
