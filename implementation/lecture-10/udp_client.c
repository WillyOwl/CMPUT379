#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef NI_MAXHOST
#define NI_MAXHOST 1025
#endif

#ifndef NI_MAXSERV
#define NI_MAXSERV 32
#endif

#define BUF_SIZE 2048

int setup_client_socket(const char* host, const char* port, struct sockaddr_storage* srvaddr, socklen_t* srvlen);

void send_and_receive(int sockfd, const struct sockaddr_storage* srvaddr, socklen_t srvlen, const char* msg);

void udp_client(const char* host, const char* port, const char* msg);


int setup_client_socket(const char* host, const char* port, struct sockaddr_storage* srvaddr, socklen_t* srvlen) {
    struct addrinfo hints, *res, *rp;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    int rc = getaddrinfo(host, port, &hints, &res);

    if (rc != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        return -1;
    }

    int fd = -1;
    for (rp = res; rp; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

        if (fd != -1) {
            memcpy(srvaddr, rp->ai_addr, rp->ai_addrlen);

            *srvlen = rp->ai_addrlen;
            break;
        }
    }

    freeaddrinfo(res);

    return fd;
}

void send_and_receive(int sockfd, const struct sockaddr_storage* srvaddr, socklen_t srvlen, const char* msg) {
    size_t len = strlen(msg);
    ssize_t sent = sendto(sockfd, msg, len, 0, (const struct sockaddr*)srvaddr, srvlen);

    if (sent < 0) {
        perror("sendto");
        return;
    }

    fprintf(stdout, "Sent %zd bytes: \"%s\"\n", sent, msg);

    struct timeval tv = {.tv_sec = 2, .tv_usec = 0};

    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char buf[BUF_SIZE + 1];
    struct sockaddr_storage from;

    socklen_t fromlen = sizeof(from);

    ssize_t n = recvfrom(sockfd, buf, BUF_SIZE, 0, (struct sockaddr*)&from, &fromlen);

    if (n < 0) {
        perror("recvfrom");
        return;
    }

    buf[n] = '\0';

    char host[NI_MAXHOST], serv[NI_MAXSERV];

    getnameinfo((struct sockaddr*)&from, fromlen, host, sizeof(host), serv, sizeof(serv), NI_NUMERICHOST | NI_NUMERICSERV);

    fprintf(stdout, "Received %zd bytes from %s:%s\n", n, host, serv);
    fprintf(stdout, "Payload: \"%s\"\n", buf);
}

void udp_client(const char* host, const char* port, const char* msg) {
    struct sockaddr_storage srvaddr;
    socklen_t srvlen = 0;

    int sockfd = setup_client_socket(host, port, &srvaddr, &srvlen);
    if (sockfd < 0) {
        perror("Client setup failed");
        exit(EXIT_FAILURE);
    }

    send_and_receive(sockfd, &srvaddr, srvlen, msg);

    close(sockfd);
}

int main(int argc, char* argv[]){

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server-host> <server-port> <message>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    udp_client(argv[1], argv[2], argv[3]);

    return 0;
}
