#include "shared_stuff.hpp"
#include "fifo_process.hpp"
#include "socket_process.hpp"

#include <cstdlib>

#include <cstring>
#include <string>
#include <vector>

#include <thread>

#include <ncurses.h>

#define HISTORY_SIZE              20
#define PANE_WIDTH                90
#define PANE_HEIGHT               57

void addToHistory(int& head, std::vector<std::string>& history, char* toAdd, std::vector<int>& dirHistory, int newDir, int& currSize){
    std::string toAddStr(toAdd, PANE_WIDTH-2);
    int prev = (head - 1 + currSize) % currSize;
    head = prev;

    std::swap(history[head], toAddStr);
    dirHistory[head] = newDir;    

    currSize++;
}

void printBuffer(WINDOW* window, int& head, std::vector<std::string>& history, std::vector<int>& dirHistory, int& currSize){
    mvwprintw(window, 1, 1, "Buffer History:          ");
    for(int i =0;i<currSize;i++){
        int index = (head + i) % currSize;
        std::string buffer = history[index];
        int bd = dirHistory[index];
        if(bd == TOSOCKET){
            mvwprintw(window, i * 3 + 2, 1, ">>>>>>>>>>>>>>>>>>>>>>>>TO SOCKET>>>>>>>>>>>>>>>>>>>>>>>>>");
            for(int i = 0;i<PANE_WIDTH-2;i++){
                mvwprintw(window, i * 3 + 3, i+1, "%c", buffer[i]);
            }
            mvwprintw(window, i * 3 + 4, 1, ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
        }
        else if(bd == TOFIFO){
            mvwprintw(window, i * 3 + 2, 1, "<<<<<<<<<<<<<<<<<<<<<<<<TO FIFO<<<<<<<<<<<<<<<<<<<<<<<<<<<");
            for(int i = 0;i<PANE_WIDTH-2;i++){
                mvwprintw(window, i * 3 + 3, i+1, "%c", buffer[i]);
            }
            mvwprintw(window, i * 3 + 4, 1, "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
        }
        else{
            mvwprintw(window, i * 3 + 2, 1, "                                                          ");
            mvwprintw(window, i * 3 + 3, 1, "Direction Value: %58d", bd);
            mvwprintw(window, i * 3 + 4, 1, "                                                          ");

        }
    }
    wrefresh(window);
}

void printPrompt(WINDOW* window, char loadingChar){
    mvwprintw(window, 1, 1, "Press q to quit or p to pause/unpause%c ", loadingChar);
    wrefresh(window);
}

void printFIFOStatus(WINDOW* window, int lastwarning){
    mvwprintw(window, 1, 1, "FIFO Status:");
    int fs = FIFO_status.load();
    char msg[PANE_WIDTH - 2];
    switch (fs){
        case FIFOSTATUS_INIT:
            std::strcpy(msg, "Initializing FIFO");
            break;
        case FIFOSTATUS_LISTENING:
            std::strcpy(msg, "FIFO opened. Awaiting \"PING\"");
            break;
        case FIFOSTATUS_EMPTY:
            std::strcpy(msg, "Connected And Awaiting query");
            break;
        case FIFOSTATUS_BUSY:
            std::strcpy(msg, "Query processing. Pinging for results.");
            break;
        case FIFOSTATUS_BADPOLL:
            std::strcpy(msg, "Polling failed");
            break;
        case FIFOSTATUS_CLOSED:
            std::strcpy(msg, "FIFO failed to closure");
            break;
        case FIFOSTATUS_BADCONNECT:
            std::strcpy(msg, "Don't trust connection because no PING");
            break;
        case FIFOSTATUS_NOCLIENT:
            std::strcpy(msg, "Nobody is listening to FIFO. Where's webapp?");
            break;
        default:
            std::strcpy(msg, "Status is undefined...");
    }
    mvwprintw(window, 4, 1, "%88s", msg);

    mvwprintw(window, 20, 1, "LAST WARNING: ");
    mvwprintw(window, 23, 1, "%88s", interpret_warning(lastwarning).c_str());
    mvwprintw(window, 24, 1, "Value %82d", lastwarning);

    wrefresh(window);
}

void printSocketStatus(WINDOW* window, int lastwarning){
    mvwprintw(window, 1, 1, "Socket Status:");
    int ss = socket_state.load();
    char msg[PANE_WIDTH-2];
    switch (ss){
        case SOCKET_LOADING:
            std::strcpy(msg, "Loading socket...");
            break;
        case SOCKET_OPENED:
            std::strcpy(msg, "Loaded socket. Awaiting connection");
            break;
        case SOCKET_CONNECTED:
            std::strcpy(msg, "Connected to client: Pinging and Checking for queries");
            break;
        case SOCKET_REQUESTED:
            std::strcpy(msg, "Query received and sending to client");
            break;
        case SOCKET_PROCESSING:
            std::strcpy(msg, "Checking for query results");
            break;
        case SOCKET_DISCONNECTED:
            std::strcpy(msg, "Reads from the socket are empty, so disconnected");
            break;
        default:
            std::strcpy(msg, "Status is undefined...");
    }
    mvwprintw(window, 4, 1, "%88s", msg);

    mvwprintw(window, 20, 1, "LAST WARNING: ");

    mvwprintw(window, 23, 1, "%88s", interpret_warning(lastwarning).c_str());
    wrefresh(window);
}

int main(){
    initscr();
    cbreak();

    WINDOW* fifowin = newwin(PANE_HEIGHT, PANE_WIDTH, 0, 0);
    box(fifowin, 0, 0);
    wrefresh(fifowin);

    WINDOW* bufferwin = newwin(PANE_HEIGHT, PANE_WIDTH, 0, PANE_WIDTH);
    box(bufferwin, 0, 0);
    wrefresh(bufferwin);

    WINDOW* socketwin = newwin(PANE_HEIGHT, PANE_WIDTH, 0, PANE_WIDTH * 2);
    box(socketwin, 0, 0);
    wrefresh(socketwin);

    WINDOW* prompt = newwin(3, 3 * PANE_WIDTH, PANE_HEIGHT, 0);
    box(prompt, 0, 0);
    wrefresh(prompt);
    nodelay(prompt, TRUE);

    bool FIFO_started = false;
    std::thread FIFO_thread;
    int lastFIFOWarning = 0;
    bool socket_started = false;
    std::thread socket_thread;
    int lastSocketWarning = 0;

    std::string prototype(PANE_WIDTH-2, ' ');
    std::vector<std::string> history(HISTORY_SIZE, prototype);
    std::vector<int> dirHistory(HISTORY_SIZE);
    int head = 0;
    int currSize = 0;
    char local_buffer[Megabyte+1];

    int count = 0;
    int index = 0;
    std::string loading = "/-\\|";
    while(true){
        int cw = warning.load();
        if(cw < -100){
            lastSocketWarning = cw;
        }
        else{
            lastFIFOWarning = cw;
        }

        if(!FIFO_started){
            FIFO_thread = std::thread(fifo_job);
            FIFO_started = true;
        }
        printFIFOStatus(fifowin, lastFIFOWarning);

        if(!socket_started){
            socket_thread = std::thread(socket_job);
            socket_started = true;
        }
        printSocketStatus(socketwin, lastSocketWarning);

        if(bufferChanged.load()){
            readFromBuffer(local_buffer);   
            addToHistory(head, history, local_buffer, dirHistory, bufferDirection.load(), currSize);
        }
        printBuffer(bufferwin, head, history, dirHistory, currSize);

        char loadingChar = loading[index];
        printPrompt(prompt, loadingChar);
        char c = wgetch(prompt);
        if(c == 'q'){
            FIFO_status.store(SHUTDOWNCODE);
            //we have to force the socket to quit when its awaiting the connection
            //on another thread because it hangs on the 'accept' function
            if(socket_state.load() == SOCKET_OPENED){
                int sockfd = _socket_fd.load();
                close_socket(sockfd);
            }
            socket_state.store(SHUTDOWNCODE);
            break;
        }
        else if(c == 'p'){
            bool nextP = !(paused.load());
            paused.store(nextP);
        }

        //loading to indicate action
        count ++;
        if(count >= 1000){
            count = 0;
            index++;
            if(index >= 4){
                index = 0;
            }
        }
    }

    mvwprintw(prompt, 1, 1, "%268s", " ");
    mvwprintw(prompt, 1, 1, "Closing FIFO...");
    wrefresh(prompt);
    FIFO_status.store(SHUTDOWNCODE);            // sending extra codes just in case
    FIFO_thread.join();

    mvwprintw(prompt, 1, 1, "%268s", " ");
    mvwprintw(prompt, 1, 1, "Closing Socket...");
    wrefresh(prompt);
    socket_state.store(SHUTDOWNCODE);          // sending extra codes just in case
    socket_thread.join();

    delwin(fifowin);
    delwin(bufferwin);
    delwin(socketwin);
    delwin(prompt);
    endwin();

    return 0;
}