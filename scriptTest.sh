#!/bin/bash

# Configuration
PORT=8080
HOST="localhost"

echo "=== Running WebServ Parser Test Suite ==="

# Test Case 1: Headers and body sent in one single packet
# Target: Checks if the body is parsed immediately when headers are complete.
test_case_1() {
    echo "Running Case 1: Headers and Content-Length body sent together..."
    printf "POST /upload HTTP/1.1\r\nHost: localhost:8080\r\nContent-Length: 12\r\nContent-Type: text/plain\r\n\r\nHello World!" | nc -N $HOST $PORT
}

# Test Case 2: Chunk data split across network packets (Incremental Delivery)
# Target: Checks if the parser correctly stores size and resumes parsing on next packets.
test_case_2() {
    echo "Running Case 2: Chunked body split across packets..."
    (
        printf "POST /upload HTTP/1.1\r\nHost: localhost:8080\r\nTransfer-Encoding: chunked\r\nContent-Type: text/plain\r\n\r\n4\r\nTe"
        sleep 1
        printf "st\r\n0\r\n\r\n"
    ) | nc -N $HOST $PORT
}

# Test Case 3: Chunk containing CRLF (\r\n) inside the actual chunk data
# Target: Checks if the parser counts bytes instead of searching for newlines.
test_case_3() {
    echo "Running Case 3: Chunk containing CRLF inside chunk data..."
    printf "POST /upload HTTP/1.1\r\nHost: localhost:8080\r\nTransfer-Encoding: chunked\r\nContent-Type: text/plain\r\n\r\n9\r\nA\r\nB\r\nC\r\n\r\n0\r\n\r\n" | nc -N $HOST $PORT
}

# Test Case 4: Multiple Chunks
# Target: Checks if the parser correctly appends multiple separate chunks to bodyContent.
test_case_4() {
    echo "Running Case 4: Multiple chunk blocks (Hello + Space + World)..."
    printf "POST /upload HTTP/1.1\r\nHost: localhost:8080\r\nTransfer-Encoding: chunked\r\nContent-Type: text/plain\r\n\r\n5\r\nHello\r\n1\r\n \r\n5\r\nWorld\r\n0\r\n\r\n" | nc -N $HOST $PORT
}

# Test Case 5: Empty Chunked Body
# Target: Checks if the parser handles a chunked request with zero bytes of body.
test_case_5() {
    echo "Running Case 5: Empty chunked request (0\r\n\r\n)..."
    printf "POST /upload HTTP/1.1\r\nHost: localhost:8080\r\nTransfer-Encoding: chunked\r\nContent-Type: text/plain\r\n\r\n0\r\n\r\n" | nc -N $HOST $PORT
}

# Test Case 6: Content-Length: 0
# Target: Checks if the parser completes immediately with an empty bodyContent.
test_case_6() {
    echo "Running Case 6: Content-Length is 0..."
    printf "POST /upload HTTP/1.1\r\nHost: localhost:8080\r\nContent-Length: 0\r\nContent-Type: text/plain\r\n\r\n" | nc -N $HOST $PORT
}

# Test Case 7: Invalid Hex Size in Chunked
# Target: Checks if the parser correctly detects invalid chunk sizes (e.g., G\r\n) and errors out.
test_case_7() {
    echo "Running Case 7: Malformed chunk size (G\r\n)..."
    printf "POST /upload HTTP/1.1\r\nHost: localhost:8080\r\nTransfer-Encoding: chunked\r\nContent-Type: text/plain\r\n\r\nG\r\nTest\r\n0\r\n\r\n" | nc -N $HOST $PORT
}

# Test Case 8: Content-Length Body Split Across Packets
# Target: Checks if Content-Length parses incrementally when the body data arrives in separate packets.
test_case_8() {
    echo "Running Case 8: Content-Length body split across packets..."
    (
        printf "POST /upload HTTP/1.1\r\nHost: localhost:8080\r\nContent-Length: 20\r\nContent-Type: text/plain\r\n\r\nPart 1 of "
        sleep 1
        printf "the body!!"
    ) | nc -N $HOST $PORT
}

# Test Case 9: Content-Length Body with CRLF (\r\n) Inside
# Target: Checks if Content-Length counts raw bytes and doesn't get cut off by line breaks in the body.
test_case_9() {
    echo "Running Case 9: Content-Length body containing internal newlines..."
    printf "POST /upload HTTP/1.1\r\nHost: localhost:8080\r\nContent-Length: 17\r\nContent-Type: text/plain\r\n\r\nLine1\r\nLine2\r\nLine3" | nc -N $HOST $PORT
}

# Test Case 10: Content-Length Premature EOF (Body Too Small)
# Target: Checks if the server detects client disconnection when expected bytes are not fully sent.
test_case_10() {
    echo "Running Case 10: Content-Length payload too small (Client closes connection prematurely)..."
    printf "POST /upload HTTP/1.1\r\nHost: localhost:8080\r\nContent-Length: 100\r\nContent-Type: text/plain\r\n\r\nSent only 20 bytes!!" | nc -N $HOST $PORT
}

# Test Case 11: Content-Length Excess Data (Body Too Large)
# Target: Checks if the parser reads exactly Content-Length bytes and leaves extra bytes in the raw_buffer (for pipelining).
test_case_11() {
    echo "Running Case 11: Content-Length body overflow (Sends more than declared size)..."
    printf "POST /upload HTTP/1.1\r\nHost: localhost:8080\r\nContent-Length: 10\r\nContent-Type: text/plain\r\n\r\n1234567890EXTRA_BYTES_HERE" | nc -N $HOST $PORT
}

case "$1" in
    1) test_case_1 ;;
    2) test_case_2 ;;
    3) test_case_3 ;;
    4) test_case_4 ;;
    5) test_case_5 ;;
    6) test_case_6 ;;
    7) test_case_7 ;;
    8) test_case_8 ;;
    9) test_case_9 ;;
    10) test_case_10 ;;
    11) test_case_11 ;;
    *)
        echo "Usage: $0 [1|2|3|4|5|6|7|8|9|10|11]"
        echo "  1: Headers and body sent in one packet (detects 'else if' parsing hang)"
        echo "  2: Chunk split across packets (detects stateless chunk parsing crashes)"
        echo "  3: Chunk containing CRLF inside data (detects scanning CRLF parse errors)"
        echo "  4: Multiple separate chunks (tests chunk appending logic)"
        echo "  5: Empty chunked request (tests immediate termination)"
        echo "  6: Content-Length: 0 (tests immediate termination)"
        echo "  7: Malformed chunk size (tests hex validation error handling)"
        echo "  8: Content-Length body split across packets (tests incremental body reading)"
        echo "  9: Content-Length body with newlines (tests binary safety of body)"
        echo " 10: Content-Length body too small (tests EOF connection closure detection)"
        echo " 11: Content-Length body overflow (tests buffer truncation to exactly declared length)"
        ;;
esac