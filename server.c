#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_CLIENTS 4
FILE *CLIENTS[MAX_CLIENTS] = {0};
// ^ Each connected client is represented as a FILE*
// If that client is not connected, its FILE* will be NULL

// Sends the message `buf` to all connected clients except
void redistribute_message(int sender_index, char *buf) {
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (i != sender_index && CLIENTS[i] != NULL) {
            if (0 > fprintf(CLIENTS[i], "%s\n", buf) || 0 != fflush(CLIENTS[i])) {
                fclose(CLIENTS[i]);
                CLIENTS[i] = NULL;
            }
        }
    }
}

// Tries to read a message from the specified client.
int poll_message(char *buf, size_t len, int client_index) {
    if (CLIENTS[client_index] == NULL) {
        return 0; // No client connected at this index
    }

    // Attempt to read from the client
    if (NULL != fgets(buf, len, CLIENTS[client_index])) {
        printf("Received from client %d: %s", client_index, buf);
        return 1; // Successfully read a message
    }

    // Handle EOF (client disconnect)
    if (feof(CLIENTS[client_index])) {
        printf("Client %d closed the connection.\n", client_index);
        fclose(CLIENTS[client_index]);
        CLIENTS[client_index] = NULL;
        return 0;
    }

    // Handle non-blocking case: no data to read
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return 0; // No message available yet, keep the connection open
    }

    // Handle actual read error
    if (ferror(CLIENTS[client_index])) {
        perror("Error reading from client");

    }
}

// Tries to accept a new client connection.
void try_add_client(int server_fd) {
    int added = 0;
    int client_fd;

    if (-1 == (client_fd = accept(server_fd, NULL, NULL))) {
        if (EAGAIN != errno && EWOULDBLOCK != errno) {
            perror("error accepting client");
            exit(1);
        }
        return;
    }
    if (-1 == fcntl(client_fd, F_SETFL, O_NONBLOCK)) {
        perror("fcntl server_fd NONBLOCK");
        close(server_fd);
        return;
    }

    FILE *client = fdopen(client_fd, "r+");
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (CLIENTS[i] == NULL) {
            CLIENTS[i] = client;
            added = 1;
            break;
        }
    }
    if (added == 0) {
        fprintf(client, "%s\n", "server is full");
        fflush(client);
        fclose(client);
    }
}

int main_loop(int server_fd)
{
    char buf[1024];

    while (1) {
        // check each client to see if there are messages to redistribute
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (NULL == CLIENTS[i]) continue;
            if (!poll_message(buf, sizeof(buf), i)) continue;
            redistribute_message(i, buf);
        }

        // see if there's a new client to add
        try_add_client(server_fd);

        usleep(100 * 1000); // wait 100ms before checking again
    }
}

int main(int argc, char* argv[])
{
    struct sockaddr_in address;
    int server_fd;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1 aka localhost
    // ^ this is a kind of security: the server will only listen
    // for incoming connections that come from 127.0.0.1 i.e. the same
    // computer that the server process is running on.
    address.sin_port = htons(2737);

    if (-1 == bind(server_fd, (struct sockaddr *)&address, sizeof(address))) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (-1 == listen(server_fd, 5)) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    // set the server file descriptor to nonblocking mode
    // so that `accept` returns immediately if there are no pending
    // incoming connections
    if (-1 == fcntl(server_fd, F_SETFL, O_NONBLOCK)) {
        perror("fcntl server_fd NONBLOCK");
        close(server_fd);
        return 1;
    }

    int status = main_loop(server_fd);
    close(server_fd);
    return status;
}
