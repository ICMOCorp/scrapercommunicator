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

#include <ncurses.h>

//DIRECTIVE: HELPERS
//Some helper functions
bool startsWith(const std::string& s1, const std::string& s2, int size) {
    return s1.compare(0, size, s2, 0, size) == 0;
}

//DIRECTIVE: SOCKET
//Our connection to the 24 hour socketman
const int PORT = 9000;
int create_socket(WINDOW* window, int& prow, int& sockfd){
    signal(SIGPIPE, SIG_IGN);
    mvwprintw(window, prow++, 1, "\tCreating socket");
    //wrefresh(window);
    sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        mvwprintw(window, prow++, 1, "Socket creation failed");
        //wrefresh(window);
        return -1;
    }
    mvwprintw(window, prow++, 1, "socket val: %d", sockfd);
    //wrefresh(window);

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
        mvwprintw(window, prow++, 1, "\tsetsockopt(SO_REUSEADDR) failed: ");
        mvwprintw(window, prow++, 1, "\t %s", strerror(errno));
        //wrefresh(window);
        close(sockfd);
        return -1;
    }

    int finalPort = PORT;
    int bindVal = bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (bindVal < 0) {
        int tries = 1;
        while((errno == EADDRINUSE || errno == EINVAL) && tries < 100){
            //cout << "Bind failed at port " << finalPort << ":" << strerror(errno) << endl;
            tries++;
            finalPort++;
            memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET; // specifies IPv4
            server_addr.sin_port = htons(finalPort); //specifies which port
            server_addr.sin_addr.s_addr = INADDR_ANY; // means "listen at any interface" ?
            bindVal = bind(sockfd, (struct sockaddr*)& server_addr, sizeof(server_addr));
        }
        mvwprintw(window, prow++, 1, "\tBind failed");
        mvwprintw(window, prow++, 1, "\t %s", strerror(errno));
        //wrefresh(window);
        return -1;
    }

    mvwprintw(window, prow++, 1, "Socket bind successful. Now listening...");
    //wrefresh(window);
    if (listen(sockfd, 5) < 0) {
        mvwprintw(window, prow++, 1, "Listen failed");
        //wrefresh(window);
        return -1;
    }

    mvwprintw(window, prow++, 1, "Socket created and listening on port %d...", finalPort);
    //wrefresh(window);
    return 1;
}

int wait_for_socket(WINDOW* window, int& socket_fd){
    wclear(window);
    //box(window, '|', '-');
    mvwprintw(window, 1, 1, "Awaiting connection to socket");
    wrefresh(window);
    sockaddr_in client{};
    socklen_t addr_len = sizeof(client);  
    int client_sock = accept(socket_fd, (struct sockaddr*)& client, &addr_len);
    //cout << "Accept val " << client_sock << endl;
    if(client_sock == -1) {
        mvwprintw(window, 2, 1, "Error accepting client connection");
        //wrefresh(window);
        return -1;
    }
    
    mvwprintw(window, 3, 1, "Connected to a client");
    //wrefresh(window);
    
    //TODO
    //determine a password authentication before allowing queries

    //wrefresh(window);
    return client_sock;
}

//const int MSG_SIZE = 4000;
int write_to_socket(WINDOW* window, int& currRowForP, int& socket_fd, const char* msg, size_t len){
    mvwprintw(window, currRowForP++, 1,"\tWriting...");
    //wrefresh(window);
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
        string trunc = msg;
        trunc.resize(10);
        mvwprintw(window, currRowForP++, 1, "\tSending>>>\"%s\" (%d chars)", trunc.c_str(), strlen(msg)); 
        //wrefresh(window);
        
        struct pollfd pfd;
        pfd.fd = socket_fd;
        pfd.events = POLLOUT;
        int pollresults = poll(&pfd, 1, 5000);
        if(pollresults == 0){
            mvwprintw(window, currRowForP++, 1, "\tPoll got empty (client not responding)");
            //wrefresh(window);
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
        mvwprintw(window, currRowForP++, 1, "\tsent %d byte(s)", bytesSent);
        //wrefresh(window);
        totalSent += bytesSent;
    }
    //wrefresh(window);
    return totalSent;
}

int ping_socket(WINDOW* window, int& socket_fd){
    //wclear(window);
    mvwprintw(window, 1, 1, "Pinging socket connection...");
    //wrefresh(window);
    struct pollfd fds[1];
    fds[0].fd = socket_fd;
    fds[0].events = POLLIN;

    int prow = 2;
    int res = write_to_socket(window, prow, socket_fd, "ping", 4);
    if(res <= 0){
        mvwprintw(window, prow++, 1, "\tError printing to socket");
        //wrefresh(window);
        return -1;
    }

    char buffer[10];
    memset(buffer, 0, sizeof(buffer));

    int ret = poll(fds, 1, 5000);
    if(ret == -1){
        cerr << "Poll error: " << strerror(errno) << endl;
        return -1;
    }
    else if(ret == 0){
        mvwprintw(window, prow++, 1, "Timeout, no data to poll");
        //wrefresh(window);
        return 0;
    }
    ssize_t bytesReceived = recv(socket_fd, buffer, sizeof(buffer)-1, 0);

    if(bytesReceived < 0){
        cout << "ERROR: something went wrong with ping!" << endl;
        cout << "\t" << strerror(errno) << endl;
        return -1;
    }
    else if(bytesReceived == 0){
        mvwprintw(window, prow++, 1, "It seems the connection is closed");
        //wrefresh(window);
        return 0;
    }

    //wrefresh(window);
    return bytesReceived;
}

void close_socket(int& socketfd){
    shutdown(socketfd, SHUT_RDWR);
    close(socketfd);
}

//DIRECTIVE: FIFO
//Anything with FIFO

const int BUFFER_SIZE = 4096;

string IFIFOPATH_STR = string(getenv("HOME")) + "/scrapecom/query";
string OFIFOPATH_STR = string(getenv("HOME")) + "/scrapecom/response";

void removeFIFO(const string& fifoPath){
    unlink(fifoPath.c_str());
}

//is useless
void closeFIFO(const int& fd){
    close(fd);
}

//this should be called before everything stops
//This function is meant to be a process kill
// and release of resources
void closeAll(int& socketfd){
    removeFIFO(OFIFOPATH_STR);
    removeFIFO(IFIFOPATH_STR);

    close_socket(socketfd);
}

int createFIFO(const string& fifoPath) {
    if (mkfifo(fifoPath.c_str(), 0666) == -1) {
        return -1;
    }
    return 1;
}

//TODO
//Looks like the FIFO errors are improving (getting more independent)
//But when we test it with our testing service app
//  - it's hard to debug because none of the interactions are being 
//      displayed
//  - our testing service app isn't robust enough to make testing 
//      more thorough/safe
int connectToFIFO(WINDOW* window, int& IFIFO_opened, int& OFIFO_opened, struct pollfd* fds){
    //wclear(window);
    //box(window, '|', '-');
    mvwprintw(window, 1, 1, "Connect To FIFO: ");
    //wrefresh(window);
    int row = 2;
    if(!IFIFO_opened){
        int read_fd = open(IFIFOPATH_STR.c_str(), O_RDONLY | O_NONBLOCK);

        if (read_fd == -1) {
            mvwprintw(window, row++, 1, "Error making FIFO pipes");
            mvwprintw(window, row++, 1, "\t %s", strerror(errno));
            //wrefresh(window);
            //std::this_thread::sleep_for(10s);
            close(read_fd);
            IFIFO_opened = false;
            return -1;
        }

        //REF: fd set
        fds[0].fd = read_fd;
        fds[0].events = POLLIN;
        IFIFO_opened = true;
    }

    if(!OFIFO_opened){
        int write_fd = open(OFIFOPATH_STR.c_str(), O_WRONLY | O_NONBLOCK);
        if(write_fd == -1){
            if(errno == ENXIO){
                mvwprintw(window, row++ + 20, 1, "Someone isn't listening");
                mvwprintw(window, row++ + 20, 1, "on the other side");
                return 0;
            }
            else{
                mvwprintw(window, row++, 1, "Error making FIFO pipes");
                mvwprintw(window, row++, 1, " %s", strerror(errno));
                close(write_fd);
                OFIFO_opened = false;
                return -1;
            }
        }

        //REF: fd set
        fds[1].fd = write_fd;
        fds[1].events = POLLOUT;
        OFIFO_opened = true;
    }
    
    return 1;
}

//DIRECTIVE: MAIN
//processes for the entire program
int init(WINDOW* window, int& socket_fd){
    //wclear(window);
    //box(window, '|', '-');
    mvwprintw(window, 1, 1, "Initializing...");
    //wrefresh(window);
    int prow = 2;


    mvwprintw(window, prow++, 1, "Making system pipes");
    //wrefresh(window);
    int res1 = createFIFO(OFIFOPATH_STR);
    if(res1 < 0){
        mvwprintw(window, prow++, 1, "Error creating FIFO: ");
        mvwprintw(window, prow++, 1, " %s",strerror(errno));
        closeAll(socket_fd);
        return -1;
    }
    int res2 = createFIFO(IFIFOPATH_STR);
    if(res2 < 0){
        mvwprintw(window, prow++, 1, "Error creating FIFO: ");
        mvwprintw(window, prow++, 1, " %s", strerror(errno));
        closeAll(socket_fd);
        return -1;
    }

    mvwprintw(window, prow++, 1, "Success on making pipes");
    mvwprintw(window, prow++, 1, "Making sockets...");
    //wrefresh(window);

    int res = create_socket(window, prow, socket_fd);
    if(res < 0){
        mvwprintw(window, prow++, 1, "Error on making socket");
        //wrefresh(window);
        closeAll(socket_fd);
        return -1;
    }
    mvwprintw(window, prow++, 1, "Success on making sockets");
    wrefresh(window);
    return 1;
}

int ping(WINDOW* window, struct pollfd* fds, char* buffer, int& socket_fd){
    //wclear(window);
    //box(window, '|', '-');
    mvwprintw(window, 1, 1, "Querying FIFO");
    //wrefresh(window);
    int expected = 0;
    ssize_t successStatus = read(fds[0].fd, &expected, sizeof(expected));
    if (successStatus > 0) {
        int total_read = 0;
        while(total_read < expected){
            ssize_t bytesRead = read(fds[0].fd, buffer + total_read, expected - total_read);
            buffer[bytesRead + total_read] = '\0';
            //wprintw("Received query: %s", buffer);
            total_read += bytesRead;
        }
        string trunc = buffer;
        int charsRead = trunc.length();
        trunc.resize(10);
        mvwprintw(window, 2, 1, "Received query: %s (%d chars read)", trunc.c_str(), charsRead);
        //wrefresh(window);

        int prow = 3;
        if(!write_to_socket(window, prow, socket_fd, buffer, expected)){ 
            mvwprintw(window, prow++, 1, "ERROR: failed to write to socket!");  
            //wrefresh(window);
            return -1;
        }

        //TODO
        //so really we need a process of the input that we get from write_to_socket


        //Write to fifo to return the results
        int status = write(fds[1].fd, "got it!", 7);
        if(status == -1){
            mvwprintw(window, prow++, 1, "Error writing to fifo: %s", strerror(errno));
            //wrefresh(window);
            return -1;
        }
    }
    return successStatus;
}

//DIRECTIVE: THREAD
//atomic<bool> running_job(true);
void communicator_job(WINDOW* initwindow, WINDOW* window, WINDOW* userwindow, WINDOW* backwindow){
    //wclear(window);
    //wrefresh(window); wrefresh(userwindow); refresh();
    box(window, '|', '-');
    box(userwindow, '|', '-');
    mvwprintw(window, 1, 1, "Initiating the communicator");
    wrefresh(window);

    bool running_job = true;

    int socket_fd;
    struct pollfd fds[2];
    int res = init(initwindow, socket_fd);
    if(res < 0){
        mvwprintw(window, 2, 1, "Initialization fail");
        wrefresh(window);
        wrefresh(backwindow);
        wrefresh(initwindow);
        return;
    }
    mvwprintw(window, 2, 1, "Initialization success");
    //wrefresh(window);

    //REF: vars for maiin loop
    bool connectedToASocketman = false;
    int client_socket_fd = -1;

    int IFIFO_opened = 0;
    int OFIFO_opened = 0;

    auto pingClockTick = std::chrono::high_resolution_clock::now();

    //REF: main loop
    mvwprintw(window, 3, 1, "Running the communicator");
    char buffer[BUFFER_SIZE];

    //refresh();
    wrefresh(window);
    std::this_thread::sleep_for(1s);
    wclear(window);
    box(window, '|', '-');

    int windowRow = 1;
    while (running_job) {
        windowRow = 1;
        wrefresh(window);
        //refresh();
        //wclear(window);
        //wclear(userwindow);
        mvwprintw(userwindow, 1, 1,"Press q to quit: ");
        char c = wgetch(userwindow);
        if(c == 'q'){
            running_job = false;
            wprintw(userwindow, "quitting...");
            wrefresh(userwindow);
            break;
        }

        //wrefresh(userwindow); 
        //refresh();
        
        mvwprintw(window, windowRow++, 1,"Status:");
        //attempt to connect to 24hr socketman
        //wclear(backwindow);
        //CHECKOUT: FIFO_logic1
        int localfifostatus = connectToFIFO(backwindow, IFIFO_opened, OFIFO_opened, fds); 
        int fifoErrorNo = errno;

        int onlinesocketstatus = 1;
        if(!connectedToASocketman){
            client_socket_fd = wait_for_socket(backwindow, socket_fd);
            connectedToASocketman = client_socket_fd > 0;
            onlinesocketstatus = connectedToASocketman;
        }
        else {
            std::chrono::duration<double> timePassedSincePing = 
                std::chrono::high_resolution_clock::now() - pingClockTick;
            if(timePassedSincePing > 10s){
                int ret = ping_socket(backwindow, client_socket_fd) <= 0;
                if(ret){
                    connectedToASocketman = false;
                    client_socket_fd = -1;
                    onlinesocketstatus = 0;
                }
                pingClockTick = std::chrono::high_resolution_clock::now();
                windowRow++;
            }
            else{
                mvwprintw(window, windowRow++, 1, "%0.3f until next ping ", 10 - timePassedSincePing.count());
            }
        }

        if(localfifostatus > 0){
            mvwprintw(window, windowRow++, 1, "Fifo Status: O    ");
            mvwprintw(window, windowRow++, 1, "                  ");
        }
        else if(localfifostatus == 0){
            mvwprintw(window, windowRow++, 1, "Fifo Status: EMPTY");
            mvwprintw(window, windowRow++, 1, "                  ");
            wrefresh(backwindow);
        }
        else{
            mvwprintw(window, windowRow++, 1, "Fifo Status: X    ");
            mvwprintw(window, windowRow++, 1, "  %s", strerror(fifoErrorNo));
            wrefresh(backwindow);
        }

        if(onlinesocketstatus){
            mvwprintw(window, windowRow++, 1, "Socket Status: O");
        }
        else{
            mvwprintw(window, windowRow++, 1, "Socket Status: X");
        }
        
        //mvwprintw(window, windowRow++, 1, "WTF %d %d %d", localfifostatus, onlinesocketstatus, status);

        if((localfifostatus > 0) && onlinesocketstatus){ 
            continue;
        }


        //cout << "\n=>Reading the webapp for requests" << endl;

//====> ping handles both fifo and socket writing
        int ret = ping(backwindow, fds, buffer, client_socket_fd);
        if(ret < 0){
            running_job = false;
            mvwprintw(window, windowRow++, 1, "Looks like we met some issues...");
            mvwprintw(window, windowRow++, 1, "Will close in 10 secs(-ish)");
            wrefresh(window);
            std::this_thread::sleep_for(10s);
        }

        //wrefresh(window); 
        //std::this_thread::sleep_for(500ms);
    }

    closeAll(socket_fd);
}

//REF: main function
int main(){
    cout << "Don't look at me. You should be seeing 3 boxes" << endl;
    initscr();
    cbreak();
    refresh();

    WINDOW* initWin = newwin(15, 50, 0, 0);
    WINDOW* backWin = newwin(23, 35, 0, 51);
    WINDOW* statusWin = newwin(15, 50, 16, 0);
    WINDOW* userWin = newwin(3, 50, 31, 0);
    nodelay(userWin, TRUE);

    /*
    while(true){
        wclear(statusWin);
        wclear(userWin);
        wprintw(statusWin, "WTF");
        wprintw(userWin, "WTF2");
        wrefresh(statusWin);
        wrefresh(userWin);
        refresh();
    }
    */

    //thread com_thread(communicator_job, statusWin, userWin);
    communicator_job(initWin, statusWin, userWin, backWin);
    refresh();
    std::this_thread::sleep_for(2000ms);

    /*
    char c;
    while(1){
        box(userWin, '|', '-');
        wprintw(userWin, "Press q to quit:");
        c = getch();
        if(c == 'q'){
            break;
        }
        wrefresh(userWin);
        //take a break, give space for other resources
        std::this_thread::sleep_for(500ms);
    }
    */

    //running_job = false;
    //com_thread.join();
    
    delwin(initWin);
    delwin(statusWin);
    delwin(userWin);
    delwin(backWin);
    endwin();
    return 0;
}