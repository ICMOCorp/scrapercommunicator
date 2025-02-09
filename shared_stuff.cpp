#include "shared_stuff.hpp"

#include <cstring>

std::atomic<bool> paused(false);

std::mutex mtx;
std::atomic<int> bufferDirection(0);
std::atomic<bool> bufferChanged(false);
char buffer[Megabyte + 1];
std::atomic<int> warning(0);

uint32_t readInteger(char* buffer){
    uint32_t val = 0;
    for(int i = 3;i>=0;i--){
        val <<= 8;
        val |= buffer[i];
    }
    return val;
}

void writeInteger(char* buffer, uint32_t num){
    std::memset(buffer, 0, 4);
    for(int i = 0;i<4;i++){
        buffer[i] = (num >> i * 8) & 255;
    }
    buffer[4] = '\0';
}

void writeToBuffer(const char* str){
    std::lock_guard<std::mutex> lock(mtx);

    //This logic attempts to see if there are any changes 
    // while copying over the contents of str onto the buffer
    //if it finds any differences, then the buffer will be marked
    //also check the size difference as "progress" and "progres"
    // are different but wont be noted with just the given for
    // loop
    int bd = bufferDirection.load();

    int buffLen = std::strlen(buffer);
    int strLen = std::strlen(str);
    bool bfc = buffLen != strLen;  //did buffer change from str?

    for(int i = 0;i<strLen;i++){
        if(buffer[i] != str[i]){
            bfc = true;
            buffer[i] = str[i];
        }
    }
    buffer[strLen] = '\0';

    bufferChanged.store(bfc);
}

void readFromBuffer(char* res){
    std::lock_guard<std::mutex> lock(mtx);
    std::strcpy(res, buffer);
}

int pollBuffer(int desiredBD, int timeout){
    auto poll_start = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed = std::chrono::system_clock::now() - poll_start;

    while(elapsed.count() < timeout){
        if(bufferDirection.load() == desiredBD){
            return 1;
        }
        elapsed = std::chrono::system_clock::now() - poll_start;
        //QUESTION:
        //do we add a sleep here?
    }

    return 0;
}

void sendToSocket(const char* str){
    bufferDirection.store(TOSOCKET);
    writeToBuffer(str);
}
int readFromSocket(char* str){
    int dir = bufferDirection.load();
    if(dir == TOFIFO){
        readFromBuffer(str);
        return 1;
    }
    return 0;
}

void sendToFIFO(const char* str){
    bufferDirection.store(TOFIFO);
    writeToBuffer(str);
}
int readFromFIFO(char* res){
    int dir = bufferDirection.load();
    if(dir == TOSOCKET){
        readFromBuffer(res);
        return 1;
    }
    return 0;
}

std::string interpret_warning(int value){
    switch(value){
        case 0:
            return "There is no warning";
        case WARNING_BADCLEAN:
            return "When cleaning, there wasn't much to clean";
        case WARNING_BADCREATION:
            return "Creation of FIFO failed";
        case WARNING_BADREAD:
            return "While reading, FIFO closed";
        case WARNING_BADDIRECTION:
            return "FIFO direction undefined";
        case WARNING_HOW:
            return "No FIFO state is defined like this";
        case WARNING_FIFOBADOPEN:
            return "FIFO open failed";
        case WARNING_BADQUERY:
            return "Undefined query. check connection";
        case WARNING_BADMESSAGE:
            return "Got a weird message from reading FIFO.";
        case WARNING_HOWSOCKET:
            return "Undefined socket state. How did we get here?";
        case WARNING_BADPINGSEND:
            return "Sending ping failed to socket";
        case WARNING_BADOPEN:
            return "Failed to open the socket";
        case WARNING_NOCONNECT:
            return "Could not connect to the client";
        case WARNING_BADSENDQUERY:
            return "Sending the query to client failed";
        case WARNING_BADREADPROG:
            return "Failed getting a response with the progress command"; //51 characters long
        case WARNING_GETTINGPROG:
            return "Bad FIFO read from socket thread";
        case WARNING_NOPORT:
            return "Ran out of available ports";
        case WARNING_BADLISTEN:
            return "Listen command failed with sockets";
        case WARNING_BADPINGRECV:
            return "Sending ping was okay, but didnt do well on recv";
        case WARNING_DISTRUSTSOCK:
            return "Check response. Currently don't trust it.";
        case WARNING_POLLTIMEOUT:
            return "Poll timed out for FIFO";
        default:
            return "undefined warning. Check value";
    }
}

int strcomp(const char* a, const char* b, uint32_t length){
    for(int i = 0;i<length;i++){
        if(a[i] != b[i]){
            return 0;
        }
    }
    return 1;
}

int querycomp(const char* a, const char* b, uint32_t length, char delim){
    size_t aLen = std::strlen(a);
    int index = -1;
    for(int i = 0;i<aLen;i++){
        if(a[i] == delim){
            index = i+1;
            break;
        }
    }

    if(index < 0 || index >= aLen){
        return -1;
    }

    for(int i = 0;i<length;i++){
        if(a[index + i] != b[i]){
            return 0;
        }
    }
    return 1;
}