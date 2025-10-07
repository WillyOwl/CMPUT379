#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
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

#define BUF_SIZE 2048

static volatile sig_atomic_t keep_running = 1;

static void handle_sigint(int sig) {
    (void)sig;
    keep_running = 0;
}

int setup_server_socket(const char* port);
void run_server_loop(int sockfd);
void udp_server(const char* port);

int setup_server_socket(const char* port) {
    struct addrinfo hints, *res, *rp;

    /*The hints argument points to an addrinfo structure
    that specifies critieria for selecting the socket address structures returned in
    the list pointed to by res */

    /*The res argument is a pointer that points to a new addrinfo structure
    with the information requested after successful completion of the function */

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;

    int rc = getaddrinfo(NULL, port, &hints, &res);

    if (rc != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        return -1;
    }

    int fd = -1;

    for (rp = res; rp; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        
        if (fd == -1) continue;

        int yes = -1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        if (bind(fd, rp->ai_addr, rp->ai_addrlen) == 0) break; // success

        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);

    return fd;
}

void run_server_loop(int sockfd) {
    char buf[BUF_SIZE];
    struct sockaddr_storage cliaddr;
    socklen_t cliaddrlen = sizeof(cliaddr);

    fprintf(stdout, "UDP server ready. Waiting for datagrams...\n");

    while (keep_running) {
        ssize_t n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr*)&cliaddr, &cliaddrlen);

        if (n < 0) {
            if (errno == EINTR) break;
            perror("recvfrom");
            continue;
        }

        char host[NI_MAXHOST], serv[NI_MAXSERV];

        getnameinfo((struct sockaddr*)&cliaddr, cliaddrlen, host, sizeof(host), serv, sizeof(serv), NI_NUMERICHOST | NI_NUMERICSERV);

        fprintf(stdout, "Received %zd bytes from %s:%s\n", n, host, serv);

        ssize_t sent = sendto(sockfd, buf, n, 0, (struct sockaddr*)&cliaddr, cliaddrlen);

        if (sent < 0) perror("sendto");
    }
}

void udp_server(const char* port) {
    struct sigaction sa = {0};

    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);

    int sockfd = setup_server_socket(port);

    if (sockfd < 0) {
        perror("Server setup failed");
        exit(EXIT_FAILURE);
    }

    run_server_loop(sockfd);

    close(sockfd);
    fprintf(stdout, "Server shutting down.\n");
}


int main(int argc, char* argv[]){

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    udp_server(argv[1]);

    return 0;
}
