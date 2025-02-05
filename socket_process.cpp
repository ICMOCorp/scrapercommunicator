#include "socket_process.hpp"
#include "shared_stuff.hpp"

std::atomic<int> socket_state(SOCKET_LOADING);

std::atomic<int> _socket_fd(-1);

std::atomic<int> connected_port(-1);

int isNumber(const char* s){
    uint32_t msgLength = std::strlen(s);
    if(msgLength == 0){
        return 0;
    }
    if(msgLength == 1){
        return s[0] >= '0' || s[0] <= '9';
    }
    int index = 0;
    if(s[0] == '-'){
        index = 1;
    }
    for(;index < msgLength;index++){
        if(s[index] < '0' || s[index] > '9'){
            return 0;
        }
    }
    return 1;
}

int open_listening(int& sockfd){
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1){
        return -1;
    }

    // Set the socket to non-blocking mode
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    //I'm just throwing in a loop to try
    // the different ports
    int foundConnection = 0;
    sockaddr_in server_addr;
    for(int p = STARTING_PORT;p < STARTING_PORT + 100; p++){
        std::memset(&server_addr, 0, sizeof(server_addr));

        //not exactly sure what's happening here...
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(p);
        server_addr.sin_addr.s_addr = INADDR_ANY;

        if(bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) >= 0){
            connected_port.store(p);
            foundConnection = 1;
            break;
        }
    }
    if(!foundConnection){
        close_socket(sockfd);
        return -2;
    }

    int backlogs = 1; // how many backlogs we are going to allow
    if(listen(sockfd, backlogs) < 0){
        close_socket(sockfd);
        return -3;
    }

    _socket_fd.store(sockfd);

    return 1;
}

int establish_connection(int& socketfd, int& clientfd){
    sockaddr_in client;
    socklen_t client_len = sizeof(client);
    std::memset(&client, 0, client_len);

    clientfd = accept(socketfd, (struct sockaddr*)& client, &client_len);
    if(clientfd == -1){
        return -1;
    }
    return 1;
}

int ping_connection(struct pollfd* clientfds, char* buffer){
    int clientfd = clientfds[0].fd;
    std::strcpy(buffer, "PING");
    int res = send_to_client(clientfd, buffer);
    if(res == -1){
        return -1;
    }

    res = read_from_client(clientfds, buffer);
    if(res == -1){
        return -2;
    }

    if(res == 0 || !strcomp(buffer, "PONG", 4)){
        return 0;
    }
    return 1;
}

int send_to_client(int& clientfd, const char* msg){
    uint32_t msgLength = std::strlen(msg);
    char lenAsStr[5];
    writeInteger(lenAsStr, msgLength);

    size_t totalSent = 0;
    while(totalSent < sizeof(lenAsStr)){
        ssize_t res = send(clientfd, lenAsStr + totalSent, sizeof(lenAsStr) - totalSent, 0);
        if(res == -1){
            return -1;
        }

        totalSent += res;
    }

    totalSent = 0;
    while(totalSent < msgLength){
        ssize_t bytesSent = send(clientfd, msg + totalSent, msgLength - totalSent, 0);
        if(bytesSent < 0){
            return -1;
        }
        else if(bytesSent == 0){
            //TODO
            // do we need to define this?
        }
        totalSent += bytesSent;
    }
    return 1;
}

int read_from_client(struct pollfd* fds, char* msg){
    int res = poll(fds, 1, 5000);
    if(res == -1){
        return -1;
    }
    else if(res == 0){ //TIMEOUT
        return 0;
    }

    char msgLengthPtr[5];

    uint32_t totalRead = 0;
    while(totalRead < 4){
        ssize_t bytesRead = recv(fds[0].fd, msgLengthPtr + totalRead, 4 - totalRead, 0);
        if(bytesRead < 0){
            return -1;
        }
        totalRead += bytesRead;
    }
    uint32_t msgLength = readInteger(msgLengthPtr);

    std::memset(msg, 0, sizeof(msg)-1);

    totalRead = 0;
    while(totalRead < msgLength){
        ssize_t bytesRead = recv(fds[0].fd, msg + totalRead, sizeof(msg) - totalRead, 0);
        if(bytesRead < 0){
            return -1;
        }
        else if(bytesRead == 0){
            //TODO
            // do I have to verify this?
        }
        totalRead += bytesRead;
    }

    return 1;
}

//is there every a case where closing the socket 
// fails? (i.e. fd == -1)
int close_socket(int& fd){
    if(fd != -1){
        shutdown(fd, SHUT_RDWR);
        close(fd);
        fd = -1;
        _socket_fd.store(-1);
    }

    return 1;
}

int socket_error(int& sockfd, int& clientfd){
    int res = close_socket(sockfd);
    if(res == -1){
        return -1;
    }
    res = close_socket(clientfd);
    if(res == -1){
        return -1;
    }
    connected_port.store(-1);
    if(socket_state.load() != SHUTDOWNCODE){
        socket_state.store(SOCKET_DISCONNECTED);
    }
    return 1;
}

void socket_job(){
    int socketFD, clientFD;
    struct pollfd clientFDS[1];

    char local_buffer[Megabyte+1];
    memset(local_buffer, '\0', sizeof(local_buffer)-1);

    while(socket_state.load() != SHUTDOWNCODE){
        int state = socket_state.load();
        if(paused.load()) {continue;}
        if(state == SOCKET_LOADING
                || state == SOCKET_DISCONNECTED){
            int res = open_listening(socketFD);
            if(res == -1){
                warning.store(WARNING_BADOPEN);
                continue;
            }
            else if(res == -2){
                warning.store(WARNING_NOPORT);
                continue;
            }
            else if(res == -3){
                warning.store(WARNING_BADLISTEN);
                continue;
            }
            socket_state.store(SOCKET_OPENED);
        }
        else if(state == SOCKET_OPENED){
            int res = establish_connection(socketFD, clientFD);
            if(res == -1){
                //socket_error(socketFD, clientFD);
                warning.store(WARNING_NOCONNECT);
                continue;
            }

            clientFDS[0].fd = clientFD;
            clientFDS[0].events = POLLIN;
            socket_state.store(SOCKET_CONNECTED);
        }
        else if(state == SOCKET_CONNECTED){
            bool bfc = bufferChanged.load();
            if(bufferChanged.load() && readFromFIFO(local_buffer)){
                socket_state.store(SOCKET_REQUESTED);
            }
            else{
                int res = ping_connection(clientFDS, local_buffer);
                if(res == -1){
                    socket_error(socketFD, clientFD);
                    warning.store(WARNING_BADPINGSEND);
                }
                else if(res == -2){
                    socket_error(socketFD, clientFD);
                    warning.store(WARNING_BADPINGRECV);
                }
                else if(res == 0){
                    socket_error(socketFD, clientFD);
                    warning.store(WARNING_DISTRUSTSOCK);
                }
            }
        }
        else if(state == SOCKET_REQUESTED){
            if(std::strlen(local_buffer) > 7 
                && querycomp(local_buffer, "search ", 7, ':') ||
                std::strlen(local_buffer) > 9 
                && querycomp(local_buffer, "analysis ", 9, ':')){
                    int res = send_to_client(clientFD, local_buffer);
                    if(res == -1){
                        socket_error(socketFD, clientFD);
                        warning.store(WARNING_BADSENDQUERY);
                        continue;
                    }
                    socket_state.store(SOCKET_PROCESSING);
            }
            else{
                sendToFIFO("ACK:Bad Query");
                socket_state.store(SOCKET_CONNECTED);
            }
        }
        else if(state == SOCKET_PROCESSING){
            if(readFromFIFO(local_buffer)){
                if(std::strlen(local_buffer) == 8 
                    && strcomp(local_buffer, "progress", 8)){
                        send_to_client(clientFD, local_buffer);
                }

                int res = read_from_client(clientFDS, local_buffer);
                int tries = 0;
                while(res == 0 && tries < 100){
                    res = read_from_client(clientFDS, local_buffer);
                    tries++;
                }
                if(tries >= 100 || res == -1){
                    socket_error(socketFD, clientFD);
                    warning.store(WARNING_BADREADPROG);
                }

                if(isNumber(local_buffer)){
                    sendToFIFO(local_buffer);
                }
                else if(std::strlen(local_buffer) > 6 
                        && strcomp(local_buffer, "RESULT", 6)){
                            sendToFIFO(local_buffer);
                            socket_state.store(SOCKET_CONNECTED);
                }
                else{
                    socket_error(socketFD, clientFD);
                    warning.store(WARNING_DISTRUSTSOCK);
                }
            }
            else{
                //If we didnt get anything from FIFO
                //this thread doesnt care 
                // but it is worth noting
                warning.store(WARNING_GETTINGPROG);
            }
        }
        else{
            //How did we even get here?
            socket_error(socketFD, clientFD);
            warning.store(WARNING_HOWSOCKET);
        }
    }
}