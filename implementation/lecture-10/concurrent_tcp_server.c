#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef NI_MAXHOST
#define NI_MAXHOST 1025
#endif

#ifndef NI_MAXSERV
#define NI_MAXSERV 32
#endif

static volatile sig_atomic_t keep_running = 1;

static void on_term(int sig) {
    (void)sig;
    keep_running = 0;
}

// Handles SIGINT / SIGERM

static void on_sigchld(int sig) {
    (void)sig;
    // Reap all dead children; use a loop in case mutiple exit at once

    int saved = errno;

    /*Every time you make a system call or library function in C
    (like recv, accept, fork, etc), the system may set a global variable called errno
    to indicate what kind of error happened.
    
    errno keeps the most recent error code from a system / library call
    
    If a signal arrives while you main program is doing something, your signal handler
    runs asynchronously (interrupting your code).
    
    If that handler calls any function that itself modifies errno (like waitpid, write, etc)
    
    it will overwrite whatever errno value your interrupted code had*/

    while (1) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);

        if (pid < 0) break;
    }

    errno = saved;
}

static ssize_t send_all(int fd, const void* buf, size_t len) {
    const char* p = (const char*)buf;

    ssize_t sent = 0;

    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }

        sent += (size_t)n;
    }

    return (ssize_t)sent;
}

static void handle_client_echo(int cfd) {
    struct sockaddr_storage ss;
    socklen_t slen = sizeof(ss);
    char host[NI_MAXHOST], serv[NI_MAXSERV];

    if (getpeername(cfd, (struct sockaddr*)&ss, &slen) == 0 && 
        getnameinfo((struct sockaddr*)&ss, slen, host, sizeof(host), serv, sizeof(serv), NI_NUMERICHOST | NI_NUMERICSERV) == 0)
            
        fprintf(stderr, "[child %d] Connected: %s %s\n", getpid(), host, serv);

    char buf[4096];

    while (1) {
        ssize_t n = recv(cfd, buf, sizeof(buf), 0);
        if (n == 0) break; // client closed

        if (n < 0) {
            if (errno == EINTR) continue; // interrupted, retry
            perror("recv");
            break;
        }

        if (send_all(cfd, buf, (size_t)n) < 0) {
            perror("send");
            break;
        }
    }

    fprintf(stderr, "[child %d] Disconnected\n", getpid());
}

static int make_listener(const char* port) {
    struct addrinfo hints, *res = NULL, *rp;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = IPPROTO_TCP;

    int rc = getaddrinfo(NULL, port, &hints, &res);

    if (rc != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        return -1;
    }

    int lfd = -1;
    for (rp = res; rp; rp = rp->ai_next) {
        lfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (lfd < 0) continue;

        int yes = 1;
        if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
            perror("setsockopt");
            close(lfd);
            lfd = -1;

            continue;
        }

        #ifdef IPV6_V6ONLY
            if (rp->ai_family == AF_INET6) {
                int no = 0;
                setsockopt(lfd, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no));
            }
        #endif

        if (bind(lfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            if (listen(lfd, SOMAXCONN) == 0) break;
            perror("listen");
        }

        else perror("bind");

        close(lfd);

        lfd = -1;
    }

    freeaddrinfo(res);
    return lfd;

}

static void install_signals() {
    struct sigaction sa;

    //SIGINT / SIGTERM: graceful shutdown

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = on_term;
    sigemptyset(&sa.sa_mask);

    // Do NOT set SA_RESTART here; we want accept() to return EINTR when stopping

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // SIGCHLD: reap children; SA_RESTART to avoid spurious EINTR on accept/IO

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);
}

int tcp_server_fork(const char* port) {
    int lfd = make_listener(port);

    if (lfd < 0) return EXIT_FAILURE;

    fprintf(stderr, "[*] Listening on port %s (process-per-connection)\n", port);

    while (keep_running) {
        struct sockaddr_storage ss;

        socklen_t slen = sizeof(ss);
        int cfd = accept(lfd, (struct sockaddr*)&ss, &slen);

        if (cfd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(cfd);
            continue; // Kepp server running even if fork fails
        }

        if (pid == 0) {
            // Child: handle client

            close(lfd);
            handle_client_echo(cfd);
            close(cfd);
            _exit(0);
        }

        else close(cfd); // Parent: close client fd and continue accepting
    }

    close(lfd);

    fprintf(stderr, "[*] Server shut down.\n");
    return EXIT_SUCCESS;
}

int main(int argc, char* argv[]) {
    const char* port = (argc > 1)? argv[1]: "8080";
    install_signals();

    return tcp_server_fork(port);
}