To give you a clear roadmap, we can divide the internal workflow into four distinct phases: Parsing & Routing, Static Method Execution, CGI Execution, and Response Construction.                             
                                                                                                                                                                                                                
  Here is the step-by-step logical flow of how your server should handle these operations.                                                                                                                      
  ──────                                                                                                                                                                                                        
  ### Phase 1: Parsing and Routing                                                                                                                                                                              
                                                                                                                                                                                                                
  Before executing a method, your server must understand what is being requested.                                                                                                                               
                                                                                                                                                                                                                
  1. Read from Socket: Read the incoming HTTP request from the client socket. If you are building a non-blocking server (using select, poll, epoll, or kqueue), you will append read bytes to a request buffer  
  until you find the \r\n\r\n sequence, which marks the end of the HTTP headers.                                                                                                                                
  2. Parse Headers: Extract the Request Line (Method, URI, HTTP Version) and the headers (e.g., Content-Length, Content-Type, Transfer-Encoding).                                                               
  3. Read Body (If Applicable): If the method is POST and a Content-Length is provided, continue reading from the socket until the entire body is buffered.                                                     
  4. Routing: Map the requested URI to a physical path on your server's filesystem based on your server configuration (e.g., resolving aliases, root directories, and index files).                             
  5. Determine Target Type: Decide if the target path maps to a static file/directory, or if it triggers a CGI script (usually determined by the file extension like .php or .py, or a specific location like   
  /cgi-bin/).                                                                                                                                                                                                   
  ──────                                                                                                                                                                                                        
  ### Phase 2: Standard Static Methods (GET, POST, DELETE)                                                                                                                                                      
                                                                                                                                                                                                                
  If the request is not a CGI script, it is handled internally by the server.                                                                                                                                   
                                                                                                                                                                                                                
  #### GET Method                                                                                                                                                                                               
                                                                                                                                                                                                                
  1. Validation: Use stat() to check if the target exists and if it is a regular file or a directory.                                                                                                           
  2. Permissions: Use access() to check if the file has read permissions (R_OK). If not, prepare a 403 Forbidden response.                                                                                      
  3. Directory Handling: If the target is a directory, check if a default index file (e.g., index.html) exists. If it does, route to that file. If not, either generate an HTML directory listing (autoindex) or
  return 403 Forbidden / 404 Not Found.                                                                                                                                                                         
  4. Read File: Open the file using open(), read its contents into a buffer, and determine its MIME type based on the file extension.                                                                           
  5. Set Status: 200 OK.                                                                                                                                                                                        
                                                                                                                                                                                                                
  #### DELETE Method                                                                                                                                                                                            
                                                                                                                                                                                                                
  1. Validation: Use stat() to check if the file exists. If not, 404 Not Found.                                                                                                                                 
  2. Permissions: Check if the server process has permission to delete the file.                                                                                                                                
  3. Execution: Call unlink(target_path).                                                                                                                                                                       
  4. Set Status: 204 No Content (if successful but no body to return) or 200 OK (if you return an HTML confirmation).                                                                                           
                                                                                                                                                                                                                
  #### POST Method (Static)                                                                                                                                                                                     
                                                                                                                                                                                                                
  Note: In classic HTTP servers, POST is almost exclusively handled by CGI. However, if you are implementing direct file uploads without CGI:                                                                   
                                                                                                                                                                                                                
  1. Validation: Determine where the uploaded file should be saved.                                                                                                                                             
  2. Execution: Open a file descriptor with O_WRONLY | O_CREAT | O_TRUNC (or O_APPEND) and write the HTTP request body to the file.                                                                             
  3. Set Status: 201 Created (if a new file was made) or 200 OK.                                                                                                                                                
  ──────                                                                                                                                                                                                        
  ### Phase 3: CGI Execution Workflow (The Hard Part)                                                                                                                                                           
                                                                                                                                                                                                                
  If the router determines the request must be handled by a CGI script (e.g., a PHP file receiving a POST request), you must spawn a new process to execute the script, pass it the request body, and capture   
  its output.                                                                                                                                                                                                   
                                                                                                                                                                                                                
  #### Step 3.1: Setup Environment Variables                                                                                                                                                                    
                                                                                                                                                                                                                
  CGI scripts read headers and metadata via environment variables. You must dynamically allocate an array of C-strings (char** envp) for execve(). Required variables typically include:                        
                                                                                                                                                                                                                
  • REQUEST_METHOD (e.g., "GET", "POST")                                                                                                                                                                        
  • QUERY_STRING (the part of the URI after ?)                                                                                                                                                                  
  • CONTENT_LENGTH (from request headers)                                                                                                                                                                       
  • CONTENT_TYPE (from request headers)                                                                                                                                                                         
  • SCRIPT_FILENAME (absolute path to the script)                                                                                                                                                               
  • PATH_INFO (any extra path after the script name in the URI)                                                                                                                                                 
  • SERVER_PROTOCOL ("HTTP/1.1")                                                                                                                                                                                
                                                                                                                                                                                                                
  #### Step 3.2: Create Inter-Process Communication (Pipes)                                                                                                                                                     
                                                                                                                                                                                                                
  You need two pipes: one to send the request body to the CGI script, and one to read the response from the CGI script.                                                                                         
                                                                                                                                                                                                                
    int pipe_in[2];  // Server -> CGI (stdin)                                                                                                                                                                   
    int pipe_out[2]; // CGI -> Server (stdout)                                                                                                                                                                  
    pipe(pipe_in);                                                                                                                                                                                              
    pipe(pipe_out);                                                                                                                                                                                             
                                                                                                                                                                                                                
  #### Step 3.3: Fork the Process                                                                                                                                                                               
                                                                                                                                                                                                                
  Use fork() to branch into a Parent process (your Web Server) and a Child process (the CGI script).                                                                                                            
                                                                                                                                                                                                                
    pid_t pid = fork();                                                                                                                                                                                         
                                                                                                                                                                                                                
  #### Step 3.4: Child Process Execution (pid == 0)                                                                                                                                                             
                                                                                                                                                                                                                
  The child process must wire the pipes to standard input and standard output, then replace itself with the CGI executable.                                                                                     
                                                                                                                                                                                                                
  1. Redirect STDIN: dup2(pipe_in[0], STDIN_FILENO);                                                                                                                                                            
  2. Redirect STDOUT: dup2(pipe_out[1], STDOUT_FILENO);                                                                                                                                                         
  3. Close File Descriptors: The child must close all 4 original pipe file descriptors, as dup2 has already duplicated the necessary ones.                                                                      
  4. Execute: Call execve().                                                                                                                                                                                    
      • argv[0] is the path to the executable (e.g., /usr/bin/php-cgi or the script itself).                                                                                                                    
      • argv[1] is the path to the script file (if needed).                                                                                                                                                     
      • envp is the environment array you built in Step 3.1.                                                                                                                                                    
  5. If execve() returns, it means it failed. exit(1); immediately.                                                                                                                                             
                                                                                                                                                                                                                
  #### Step 3.5: Parent Process Execution (pid > 0)                                                                                                                                                             
  
  1. Close Unused Ends:
    close(pipe_in[0]);  // Server doesn't read from stdin pipe
    close(pipe_out[1]); // Server doesn't write to stdout pipe
    
  2. Write Body (POST): If the request has a body, write it to pipe_in[1].
  3. Send EOF: Critically, you must close(pipe_in[1]) immediately after writing the body. This sends an EOF (End-Of-File) to the CGI script's standard input. Without this, the CGI script will hang forever
  waiting for more data.
  4. Read Output: Read from pipe_out[0] into a buffer until read() returns 0 (EOF). This buffer now contains the full CGI response.                                                                             
  5. Reap the Zombie: Call waitpid(pid, &status, 0) to wait for the child to finish and clean up its process table entry.
  6. Clean up: close(pipe_out[0]);
  
  (Note for non-blocking servers: write, read, and waitpid in the parent can block your entire server. In a robust C++98 event loop, you would register pipe_in[1] and pipe_out[0] with select() and handle them
  asynchronously, and use waitpid with the WNOHANG flag).
  ──────
  ### Phase 4: Constructing and Sending the Response     
        
  Once you have the content (either from a static file read or CGI output), you must format it according to HTTP/1.1 specifications.
  
  #### For Static Methods (GET, POST, DELETE)
  
  You manually build the entire response string:
  
  1. Status Line: HTTP/1.1 200 OK\r\n           
  2. Headers:
      • Content-Type: text/html\r\n (based on extension)
      • Content-Length: 1024\r\n (based on file size)
      • Connection: keep-alive\r\n
  3. Empty Line: \r\n
  4. Body: Append the raw file buffer.
  5. Write the entire buffer back to the client socket.
  
  #### For CGI Scripts
  
  CGI scripts do not generate complete HTTP responses. They generate a partial response consisting of CGI Headers, an empty line, and a body. It looks like this:
  
    Content-type: text/html\r\n
    Status: 404 Not Found\r\n    <-- Optional, CGI might dictate the status
    \r\n
    <html>...</html>
        
  Your web server must parse the string returned by pipe_out[0]:
  
  1. Find the \r\n\r\n separating the CGI headers from the CGI body.
  2. Look for a Status: header in the CGI output. If it exists, use it (e.g., HTTP/1.1 404 Not Found). If it doesn't exist, assume HTTP/1.1 200 OK.
  3. Extract the Content-Length from the CGI headers, or manually calculate the length of the body following the \r\n\r\n.
  4. Prepend the HTTP/1.1 [Status Code] [Status Message]\r\n to the top.
  5. Append any server-specific headers (like Server: webserv/1.0 or Date).
  6. Append the raw CGI headers, the empty line \r\n, and the CGI body. 
  7. Write the finalized buffer to the client socket.
