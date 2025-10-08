// rpc_client.c — send one manual RPC line and print the reply
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_LINE 4096

static int send_all(int fd, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

static ssize_t recv_line(int fd, char *dst, size_t max) {
    size_t i = 0;
    while (i + 1 < max) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) {
            if (i == 0) return 0;
            break;
        }
        dst[i++] = c;
        if (c == '\n') break;
    }
    dst[i] = '\0';
    return (ssize_t)i;
}

static int connect_to(const char *host, const char *port) {
    struct addrinfo hints, *ai = NULL, *p = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET; // keep it simple
    hints.ai_socktype = SOCK_STREAM;

    int rc = getaddrinfo(host, port, &hints, &ai);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        return -1;
    }

    int fd = -1;
    for (p = ai; p; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(ai);
    return fd;
}

int rpc_client(const char *host, const char *port, const char *reqline) {
    int fd = connect_to(host, port);
    if (fd < 0) { perror("connect"); return 1; }

    // Build request with trailing newline (protocol requires it)
    char out[MAX_LINE];
    size_t len = snprintf(out, sizeof(out), "%s\n", reqline);
    if (len >= sizeof(out)) { fprintf(stderr, "request too long\n"); close(fd); return 1; }

    if (send_all(fd, out, len) != 0) { perror("send"); close(fd); return 1; }

    char in[MAX_LINE];
    ssize_t n = recv_line(fd, in, sizeof(in));
    if (n <= 0) { fprintf(stderr, "no reply\n"); close(fd); return 1; }

    // Print the server's single-line reply as-is
    fputs(in, stdout);

    close(fd);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <host> <port> <REQUEST...>\n", argv[0]);
        fprintf(stderr, "Example: %s 127.0.0.1 5555 ADD 2 40\n", argv[0]);
        return 2;
    }
    // Join argv[3..] into one space-separated line
    char req[MAX_LINE] = {0};
    size_t pos = 0;
    for (int i = 3; i < argc; ++i) {
        size_t need = strlen(argv[i]) + (i + 1 < argc ? 1 : 0);
        if (pos + need >= sizeof(req)) { fprintf(stderr, "request too long\n"); return 1; }
        memcpy(req + pos, argv[i], strlen(argv[i]));
        pos += strlen(argv[i]);
        if (i + 1 < argc) req[pos++] = ' ';
    }
    return rpc_client(argv[1], argv[2], req);
}