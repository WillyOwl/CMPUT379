#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef NI_MAXHOST
#define NI_MAXHOST 1025
#endif

#ifndef NI_MAXSERV
#define NI_MAXSERV 32
#endif

#define BACKLOG 16
#define MAX_LINE 4096

// Trim trailing CRLF/space
static void rstrip(char* s) {
    size_t n = strlen(s);

    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r' || isspace(s[n-1])))
        s[--n] = '\0';
}

//Send all bytes in buf (handles partial sends)
static int send_all(int fd, const char* buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }

        if (n == 0) return -1; // Unexpected

        sent += (size_t)n;
    }

    return 0;
}

// Read a single line into dst (up to max-1 chars). Returns number of bytes or -1 on error, 0 on EOF
static ssize_t recv_line(int fd, char* dst, size_t max) {
    size_t i = 0;
    while (i + 1 < max) { // leave space for '\0'
        char c;
        ssize_t n = recv(fd, &c, 1, 0);

        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }

        if (n == 0) {// EOF
            if (i == 0) return 0; // no data read
            break;
        }

        dst[i++] = c;
        if (c == '\n') break; // Stop at newline
    }

    dst[i] = '\0';

    return (ssize_t)i;
}

//Desensitize string compare for ASCII
static int insensitive_str(const char* a, const char* b) {
    for (; *a && *b; ++a, ++b) {
        int insen_a = tolower(*a);
        int insen_b = tolower(*b);

        if (insen_a != insen_b) return insen_a - insen_b;
    }

    return *a - *b;
}

// Parse two integers from tokens t1, t2 -> *x, *y (where x and y are operands); return 0 on success otherwise -1
static int parse_two(const char* t1, const char* t2, long long* x, long long* y) {
    if (!t1 || !t2) return -1;

    char* end = NULL;

    errno = 0; long long a = strtoll(t1, &end, 10);
    if (errno || end == t1 || *end != '\0') return -1;

    errno = 0; long long b = strtoll(t2, &end, 10);
    if (errno || end == t2 || *end != '\0') return -1;

    *x = a; *y = b;

    return 0;
}

// Handle one request line and write a reply into out/outsz. Return 0 on success otherwise
static int handle_request_line(const char* line, char* out, size_t outsz) {
    char buf[MAX_LINE];
    strncpy(buf, line, sizeof(buf));

    buf[sizeof(buf) - 1] = '\0';

    rstrip(buf);
    if (buf[0] == '\0') {
        snprintf(out, outsz, "ERROR empty request\n");
        return 0;
    }

    char* save = NULL;
    char* cmd = strtok_r(buf, " \t", &save);

    if (!cmd) {
        snprintf(out, outsz, "ERROR parse error\n");
        return 0;
    }

    if (insensitive_str(cmd, "QUIT") == 0) {
        snprintf(out, outsz, "RESULT bye\n");
        return 0;
    }

    if (insensitive_str(cmd, "ADD") == 0 || insensitive_str(cmd, "SUB") == 0 ||
        insensitive_str(cmd, "MUL") == 0 || insensitive_str(cmd, "DIV") == 0) {
            char* t1 = strtok_r(NULL, " \t", &save);
            char* t2 = strtok_r(NULL, " \t", &save);

            long long x, y;
            if (parse_two(t1, t2, &x, &y) != 0) {
                snprintf(out, outsz, "ERROR need two integers\n");
                return 0;
            }

            long long result = 0;

            if (insensitive_str(cmd, "ADD") == 0) result = x + y;
            else if (insensitive_str(cmd, "SUB") == 0) result = x - y;
            else if(insensitive_str(cmd, "MUL") == 0) result = x * y;
            else {
                if (y == 0) {
                    snprintf(out, outsz, "ERROR divided by zero\n");
                    return 0;
                }
                result = x / y;
            }

            snprintf(out, outsz, "RESULT %lld\n", result);
            return 0;
        }

        snprintf(out, outsz, "ERROR Unknown command\n");
        return 0;
}

static int create_listen_socket(const char* port) {
    struct addrinfo hints, *ai = NULL, *p = NULL;
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int rc = getaddrinfo(NULL, port, &hints, &ai);

    if (rc != 0) {
        fprintf(stderr, "geraddrinfo: %s\n", gai_strerror(rc));
        return -1;
    }

    int fd = -1;

    for (p = ai; p; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

        if (fd < 0) continue;

        int yes = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        if (bind(fd, p->ai_addr, p->ai_addrlen) == 0) break;

        close(fd);
        fd = -1;
    }

    freeaddrinfo(ai);

    if (fd < 0) return -1;
    if (listen(fd, BACKLOG) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static void serve_client(int cfd) {
    char line[MAX_LINE];
    char reply[MAX_LINE];

    while (1) {
        ssize_t n = recv_line(cfd, line, sizeof(line));
        if (n < 0) {
            perror("recv");
            break;
        }

        if (n == 0) break;

        if (handle_request_line(line, reply, sizeof(reply)) != 0) 
            snprintf(reply, sizeof(reply), "ERROR internal\n");

        if (send_all(cfd, reply, strlen(reply)) != 0) {
            perror("send");
            break;
        }
    }
}

int rpc_server(const char* port) {
    int lfd = create_listen_socket(port);
    if (lfd < 0) { perror("listen socket"); return 1; }

    printf("RPC server listening on port %s\n", port);

    while (1) {
        struct sockaddr_storage ss;
        socklen_t slen = sizeof(ss);
        int cfd = accept(lfd, (struct sockaddr *)&ss, &slen);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }
        serve_client(cfd);
        close(cfd);
    }
    close(lfd);
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 2;
    }
    
    return rpc_server(argv[1]);
}