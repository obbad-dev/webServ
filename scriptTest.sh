printf "POST /upload HTTP/1.1\r\n\
Host: localhost:8080\r\n\
Transfer-Encoding: chunked\r\n\
Content-Type: text/plain\r\n\
\r\n\
4\r\n\
Test\r\n\
7\r\n\
ing 123\r\n\
B\r\n\
 webServ!!!\r\n\
0\r\n\
\r\n" | nc localhost 8080