#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

int main() {
    int fd[2];
    if (pipe(fd) == -1) {
        perror("pipe");
        exit(1);
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        // Child: supposed to read

        // Close unused write end in child
        close(fd[1]);

        while (1) {
            char buf[128];
            ssize_t n = read(fd[0], buf, sizeof(buf));
            if (n == -1) {
                perror("read");
                exit(1);
            }
        }

        // Print what we got
        write(STDOUT_FILENO, buf, n);

        // Now try to read again until EOF
        n = read(fd[0], buf, sizeof(buf));  // <--- will block forever
        // we never reach here

        close(fd[0]);
        exit(0);
    } else {
        // Parent: writes
        while (1) {
            const char *msg = "hello\n";
            write(fd[1], msg, 6);
        }
        close(fd[1]); // close write end to signal EOF to child

        // BUG: parent keeps fd[1] open and exits without closing
        close(fd[0]);  // often also left open in bad code

        // Just wait for child (child will deadlock on second read)
        wait(NULL);
    }
}
