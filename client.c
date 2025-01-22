#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char *argv[])
{
    struct sockaddr_in address;
    int sock_fd;
    char buf[1024];

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1 aka localhost
    address.sin_port = htons(2737);


    if (-1 == connect(sock_fd, (struct sockaddr *)&address, sizeof(address))) {
        perror("connect");
        return 1;
    }

    // make stdin nonblocking:
    if (-1 == fcntl(fileno(stdin), F_SETFL, O_NONBLOCK)) {
        perror("fcntl stdin NONBLOCK");
        return 1;
    }



    // make the socket nonblocking:
    if (-1 == fcntl(sock_fd, F_SETFL, O_NONBLOCK)) {
        perror("fcntl sock_fd NONBLOCK");
        return 1;
    }




    FILE *server = fdopen(sock_fd, "r+");

    while (1) {
        // - Completes the below polling loop:
        //      - try to read from stdin, and forward across the socket
        //      - try to read from the socket, and forward to stdout

        if (NULL != fgets(buf, sizeof(buf), stdin)) {
            if (feof(server)) {
                printf("Server closed the connection.\n");
                break;
            }
            if (fprintf(server, "%s", buf) < 0 || fflush(server) < 0) {
                perror("fprintf or fflush");
                fclose(server);
                return 1;
            }
        } else {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("fgets socket");
                fclose(server);
                return 1;
            }
        }
        if (NULL != fgets(buf, sizeof(buf), server)) {
            if (strncmp(buf, "server is full", 14) == 0) {
                printf("exiting: server is full.\n");
                fclose(server);
                return 0;
            }
            if (printf("%s", buf) < 0) {
                perror("printf");
                fclose(server);
                return 1;
            }
        } else {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("fgets server");
                fclose(server);
                return 1;
            }
        }

        usleep(100 * 1000); // wait 100ms before checking again
    }

    return 0;
}
