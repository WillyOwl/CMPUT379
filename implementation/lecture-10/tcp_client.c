#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

void tcp_client() {
    int sock_fd;
    struct sockaddr_in server_addr;

    char buffer[BUFFER_SIZE];
    const char* msg = "Hello from client!\n";
    ssize_t bytes_received;

    //Create socket
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    //Set up server info
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    //Connect to server
    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect failed");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    //Send message
    send(sock_fd, msg, strlen(msg), 0);
    printf("Message sent: %s", msg);

    //Receive echo
    bytes_received = recv(sock_fd, buffer, BUFFER_SIZE, 0);

    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("Echoed from server: %s", buffer);
    }

    //Close
    close(sock_fd);
}

int main(){
    tcp_client();

    return 0;
}