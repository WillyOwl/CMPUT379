#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static ssize_t write_all (int fd, const void* buf, size_t count) {
    const char* p = (const char*)buf;
    size_t left = count;

    while (left > 0) {
        ssize_t n = write(fd, p, left);

        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }

        p += n;
        left -= (size_t)n;

    }

    return (ssize_t)count;
}

static void consumer (int read_fd) {
    char buf[4096];
    char line[4096];
    size_t linelen = 0;

    while (1) {
        ssize_t n = read(read_fd, buf, sizeof buf);

        if (n < 0) {
            if (errno == EINTR) continue;
            perror("read");
            break;
        }

        if (n == 0) {
            // EOF: producer closes write end

            break;
        }

        for (ssize_t i = 0; i < n; ++i) {
            if (linelen < sizeof(line) - 1)
                line[linelen++] = buf[i];

            if (buf[i] == '\n') {
                line[linelen] = '\0';

                // "Consume" the item (print it)

                fputs(line, stdout);
                linelen = 0;
            }
        }
    }

    if (linelen > 0) {
        line[linelen] = '\0';
        fputs(line, stdout);
    }
}

static void producer (int write_fd, int N) {
    signal(SIGPIPE, SIG_IGN);

    char out[128];

    for (int i = 0; i < N; ++i) {
        int len = snprintf(out, sizeof out, "item %d\n", i);

        if (len < 0 || len >= (int)sizeof out) {
            fprintf(stderr, "snprintf truncated\n");
            break;
        }

        if (write_all(write_fd, out, (size_t)len) < 0) {
            perror("write");
            break;
        }
    }
}

int main(){
    int fd[2];

    if (pipe(fd) < 0) {
        perror("pipe");
        return 1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        // Child: Consumer

        close(fd[1]);
        consumer(fd[0]);
        close(fd[0]);
        exit(0);
    }

    else {
        // Parent: Producer

        close(fd[0]);
        const int N = 100;

        producer(fd[1], N);
        close(fd[1]);

        int status = 0;
        waitpid(pid, &status, 0);

        return (WIFEXITED(status) ? WEXITSTATUS(status): 1);
    }

    return 0;
}