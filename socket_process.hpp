#pragma once

#include <cstring>
#include <string>

#include <sys/socket.h> // For socket(), bind(), 
                        //   listen(), accept(), and send() functions.
#include <netinet/in.h> // For sockaddr_in structure.
#include <arpa/inet.h> // For inet_ntoa() and htons() functions.
#include <unistd.h> // For close().
#include <poll.h>       // for poll and POLLIN
#include <fcntl.h>      //for fcntl, F_GETFL, F_SETFL, O_NONBLOCK

#include <atomic>

extern std::atomic<int> socket_state;
#define SHUTDOWNCODE             -10
#define SOCKET_DISCONNECTED       -1

#define SOCKET_LOADING             0
#define SOCKET_OPENED              1
#define SOCKET_CONNECTED           2
#define SOCKET_REQUESTED           3
#define SOCKET_PROCESSING          4

extern std::atomic<int> _socket_fd;

#define STARTING_PORT           9000
extern std::atomic<int> connected_port;

int isNumber(const char* s);

int open_listening(int& sockfd);

int establish_connection(int& socketfd, int& clientfd);

int ping_connection(struct pollfd* clientfds, char* buffer);

int send_to_client(int& clientfd, const char* msg);

int read_from_client(struct pollfd* fds, char* msg);

int close_socket(int& fd);

int socket_error(int& sockfd, int& clientfd);

void socket_job();