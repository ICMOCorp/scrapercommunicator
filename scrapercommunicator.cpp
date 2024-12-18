#include <cstdlib>

#include <iostream>
using std::cout;
using std::cin;
using std::cerr;
using std::endl;

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <cerrno>
#include <signal.h>

#include <cstring>
#include <string>
using std::string;

#include <thread>
#include <chrono>
#include <atomic>
using std::thread;
using namespace std::literals::chrono_literals;
using std::atomic;

//DIRECTIVE: HELPERS
//Some helper functions
bool startsWith(const std::string& s1, const std::string& s2, int size) {
    return s1.compare(0, size, s2, 0, size) == 0;
}

//DIRECTIVE: SOCKET
//Our connection to the 24 hour socketman
const int PORT = 9000;
void create_socket(int& sockfd){
    signal(SIGPIPE, SIG_IGN);
    cout << "Creating socket" << endl;
    sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        cerr << "Socket creation failed" << endl;
        exit(1);
    }
    cout << "socket val: " << sockfd << endl;

    server_addr.sin_family = AF_INET; // specifies IPv4
    server_addr.sin_port = htons(PORT); //specifies which port
    server_addr.sin_addr.s_addr = INADDR_ANY; // means "listen at any interface" ?

    /*
    int flags = fcntl(server_sock, F_GETFL, 0);
    if (fcntl(server_sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::cerr << "Failed to set non-blocking" << std::endl;
        close(server_sock);
        return 1;
    }
    */

    // Use setsockopt to allow address/port reuse
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        cerr << "setsockopt(SO_REUSEADDR) failed: " << strerror(errno) << endl;
        close(sockfd);
        return;
    }

    int finalPort = PORT;
    int bindVal = bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (bindVal < 0) {
        int tries = 1;
        while((errno == EADDRINUSE || errno == EINVAL) && tries < 100){
            cout << "Bind failed at port " << finalPort << ":" << strerror(errno) << endl;
            tries++;
            finalPort++;
            memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET; // specifies IPv4
            server_addr.sin_port = htons(finalPort); //specifies which port
            server_addr.sin_addr.s_addr = INADDR_ANY; // means "listen at any interface" ?
            bindVal = bind(sockfd, (struct sockaddr*)& server_addr, sizeof(server_addr));
        }
        cerr << "Bind failed" << endl;
        cout << strerror(errno) << endl;
        close(sockfd);
        exit(1);
    }

    cout << "Socket bind successful. Now listening..." << endl;
    if (listen(sockfd, 5) < 0) {
        cerr << "Listen failed" << endl;
        close(sockfd);
        exit(1);
    }

    cout << "Socket created and listening on port " << finalPort << "..." << endl;
}

int wait_for_socket(int& socket_fd){
    cout << "Awaiting connection to socket " << endl;
    sockaddr_in client{};
    socklen_t addr_len = sizeof(client);  
    int client_sock = accept(socket_fd, (struct sockaddr*)& client, &addr_len);
    cout << "Accept val " << client_sock << endl;
    if(client_sock == -1) {
        cout << "Error accepting client connection" << endl;
        return -1;
    }
    
    cout << "Connected to a client" << endl;
    
    //TODO
    //determine a password authentication before allowing queries
    return client_sock;
}

//const int MSG_SIZE = 4000;
int write_to_socket(int& socket_fd, const char* msg, size_t len){
    send(socket_fd, &len, sizeof(len), 0);
    //char tosend[len+1];
    //memset(tosend, ' );
    /*
    cout << "Want to send " << msg << endl;
    cout << "BEFORE:" << endl;
    for(int i = 0;i<10;i++){
        cout << "char code: " << (int)tosend[i] << endl;
    }
    cout << "Size of msg " << strlen(msg) << endl;
    for(int i = 0;i<strlen(msg);i++){
        tosend[i] = msg[i];
        cout << "Wrote " << (int)msg[i] << endl;
    }
    cout << "AFTER:" << endl;
    for(int i = 0;i<10;i++){
        cout << "char code: " << (int)tosend[i] << endl;
    }
    */
    //tosend[MSG_SIZE] = '\0';
    size_t totalSent = 0;
    while (totalSent < len+1) {
        cout << "Sending>>>\"" << msg << "\""<< endl;
        
        struct pollfd pfd;
        pfd.fd = socket_fd;
        pfd.events = POLLOUT;
        int pollresults = poll(&pfd, 1, 5000);
        if(pollresults == 0){
            cout << "Poll got empty signal, client is not responding" << endl;
            return 0;
        }
        else if(pollresults < 0){
            cout << "Poll error: " << strerror(errno) << endl;
            return -1;
        }
        ssize_t bytesSent = send(socket_fd, msg + totalSent, len + 1 - totalSent, 1);
        if (bytesSent < 0) {
            cout << "Could not send msg to socket: " << strerror(errno) << endl;
            return -1; // Error occurred
        }
        cout << "sent " << bytesSent << " byte(s)" << endl;
        totalSent += bytesSent;
    }
    return totalSent;
}

int ping_socket(int& socket_fd){
    cout << "Pinging socket connection..." << endl;
    struct pollfd fds[1];
    fds[0].fd = socket_fd;
    fds[0].events = POLLIN;

    int res = write_to_socket(socket_fd, "ping", 4);
    if(res <= 0){
        cout << "Error connecting to socket" << endl;
    }

    char buffer[10];
    memset(buffer, 0, sizeof(buffer));

    int ret = poll(fds, 1, 5000);
    if(ret == -1){
        cerr << "Poll error: " << strerror(errno) << endl;
        return -1;
    }
    else if(ret == 0){
        cout << "Timeout, no data to poll" << endl;
        return 0;
    }
    ssize_t bytesReceived = recv(socket_fd, buffer, sizeof(buffer)-1, 0);

    if(bytesReceived < 0){
        cout << "ERROR: something went wrong with ping!" << endl;
        cout << "\t" << strerror(errno) << endl;
        return -1;
    }
    else if(bytesReceived == 0){
        cout << "It seems the connection is closed" << endl;
        return 0;
    }

    return bytesReceived;
}

void close_socket(int& socketfd){
    cout << "Closing the connection" << endl;
    shutdown(socketfd, SHUT_RDWR);
    close(socketfd);
}

//DIRECTIVE: FIFO
//Anything with FIFO

const int BUFFER_SIZE = 4096;

string IFIFOPATH_STR = string(getenv("HOME")) + "/scrapecom/query";
string OFIFOPATH_STR = string(getenv("HOME")) + "/scrapecom/response";

void closeFIFO(const string& fifoPath){
    unlink(fifoPath.c_str());
}

void closeAll(){
    closeFIFO(OFIFOPATH_STR);
    closeFIFO(IFIFOPATH_STR);
}

void createFIFO(const string& fifoPath) {
    if (mkfifo(fifoPath.c_str(), 0666) == -1) {
        if (errno != EEXIST) {
            cerr << "Error creating FIFO: " << strerror(errno) << endl;
            closeAll();
            exit(1);
        }
    }
}


//DIRECTIVE: MAIN
//processes for the entire program
void init(struct pollfd* fds, int& socket_fd){
    signal(SIGPIPE, SIG_IGN);

    cout << "Making system pipes " << endl;
    createFIFO(OFIFOPATH_STR);
    createFIFO(IFIFOPATH_STR);

    int read_fd = open(IFIFOPATH_STR.c_str(), O_RDONLY);//| O_NONBLOCK);
    int write_fd = open(OFIFOPATH_STR.c_str(), O_WRONLY);// | O_NONBLOCK);

    if (read_fd == -1 || write_fd == -1) {
        cerr << "Error opening FIFOs" << endl;
        closeAll();
        exit(1);
    }

    cout << "Success on making pipes " << endl;

    //REF: fd set
    fds[0].fd = read_fd;
    fds[0].events = POLLIN;
    fds[1].fd = write_fd;
    fds[1].events = POLLOUT;

    create_socket(socket_fd);
}

int ping(struct pollfd* fds, char* buffer, int& socket_fd){
    //check to see if the fifo fd is worth blocking for data
    int timeout = 5000; // 5s timeout
    int ret = poll(fds, 1, timeout);
    if(ret == -1){
        cout << "ERROR: fd gives error when polling" << endl;
        return -1;
    }
    else if(ret == 0){
        cout << "Nothing on pipe" << endl;
        return 0;
    }

    int expected = 0;
    ssize_t successStatus = read(fds[0].fd, &expected, sizeof(expected));
    if (successStatus > 0) {
        int total_read = 0;
        while(total_read < expected){
            ssize_t bytesRead = read(fds[0].fd, buffer + total_read, expected - total_read);
            buffer[bytesRead + total_read] = '\0';
            cout << "Received query: " << buffer << endl;
            total_read += bytesRead;
        }

        //We can just redirect this directly to the socket
        /*
        //process the response
        string buffer_str = buffer;

        if (startsWith(buffer, "search", 6)) {
            string topic = buffer_str.substr(7);
            //TODO
            //send this query to the socket
        } else if (startsWith(buffer, "get_links", 9)) {
            string topic = buffer_str.substr(10);
            string smartTopic = buffer_str.substr(10 + topic.length() + 1);
            //TODO
            //send this query to the socket
        } else if (startsWith(buffer, "blog_data", 9)) {
            string topic = buffer_str.substr(10);
            //TODO
            //send this query to the socket
        } else {
            cerr << "Error: Unknown query!" << endl;
        }
        //TODO
        //handle the write part
        */

        if(!write_to_socket(socket_fd, buffer, expected)){
            cout << "ERROR: failed to write to socket!" << endl;
            return -1;
        }

        //TODO
        //so really we need a process of the input that we get from write_to_socket


        //Write to fifo to return the results
        int status = write(fds[1].fd, "got it!", 7);
        if(status == -1){
            std::cerr << "Error writing to fifo: " << strerror(errno) << std::endl;
            return -1;
        }
    }
    return successStatus;
}

//DIRECTIVE: THREAD
atomic<bool> running_job(true);
void communicator_job(){
    cout << "Initiating the communicator"  << endl;
    int socket_fd;
    struct pollfd fds[2];
    init(fds, socket_fd);

    bool connectedToASocketman = false;
    int client_socket_fd = -1;

    //TODO
    //we need an exit condition for the while loop
    cout << "Running the communicator" << endl;
    char buffer[BUFFER_SIZE];
    while (running_job) {
        //attempt to connect to 24hr socketman
        if(!connectedToASocketman){
            client_socket_fd = wait_for_socket(socket_fd);
            connectedToASocketman = client_socket_fd;
        }
        else if(ping_socket(client_socket_fd) <= 0){
            cout << "ping failed" << endl;
            connectedToASocketman = false;
            client_socket_fd = -1;
            continue;
        }

        //check the query and look for anything to send to socket man
        cout << "\n=>Reading the webapp for requests" << endl;
        int ret = ping(fds, buffer, client_socket_fd);
        if(ret < 0){
            running_job = false;
        }
    }

    closeAll();
}

int main(){
    thread com_thread(communicator_job);

    cout << "Remember to press q to quit:" << endl;
    char c;
    while(1){
        cin >> c;
        if(c == 'q'){
            break;
        }
        //take a break, give space for other resources
        std::this_thread::sleep_for(500ms);
    }

    cout << "Exiting the code..." << endl;
    running_job = false;
    com_thread.join();
    
    return 0;
}