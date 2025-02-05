#include "fifo_process.hpp"
#include "shared_stuff.hpp"

const std::string FIFOdirpath = (fs::path(getenv("HOME")) / "scrapecom").string();
const std::string FIFOinputpath = (fs::path(FIFOdirpath) / "query").string();
const std::string FIFOoutputpath = (fs::path(FIFOdirpath) / "response").string();

std::atomic<int> FIFO_status(-1);
std::atomic<bool> FIFO_hard_reset(false);

int cleanup_fifo(){
    if(!fs::is_directory(FIFOdirpath)) {
        return 0;
    }

    if(!fs::is_fifo(FIFOinputpath)){
        return 0;
    }

    if(!fs::is_fifo(FIFOoutputpath)){
        return 0;
    }

    unlink(FIFOinputpath.c_str());
    unlink(FIFOoutputpath.c_str());

    return 1;
}

int create_fifo(){
    if(!fs::is_directory(FIFOdirpath)) {
        fs::create_directory(FIFOdirpath);
    }

    if(!fs::is_fifo(FIFOinputpath)){
        int res = mkfifo(FIFOinputpath.c_str(), 0666);
        if(res == -1){
            // creation failed
            unlink(FIFOinputpath.c_str());
            return -1;
        }
    }

    if(!fs::is_fifo(FIFOoutputpath)){
        int res = mkfifo(FIFOoutputpath.c_str(), 0666);
        if(res == -1){
            //creation failed
            unlink(FIFOoutputpath.c_str());
            return -1;
        }
    }

    return 1;
}

void close_fifo(struct pollfd* readPFD, struct pollfd* writePFD){
    if(readPFD->fd != -1){
        close(readPFD->fd);
    }
    
    if(writePFD->fd != -1){
        close(writePFD->fd);
    }
}

int open_fifo(struct pollfd* readPFD, struct pollfd* writePFD){
    int writefd = open(FIFOoutputpath.c_str(), O_WRONLY | O_NONBLOCK);
    if(writefd == -1){
        return 0;
    }
    writePFD->fd = writefd;
    writePFD->events = POLLOUT;

    int readfd = open(FIFOinputpath.c_str(), O_RDONLY | O_NONBLOCK);
    if(readfd == -1){
        return -1;
    }
    readPFD->fd = readfd;
    readPFD->events = POLLIN;

    return 1;
}

int poll_fifo(struct pollfd* readPFD, int timeout){
    int res = poll(readPFD, 1, timeout);
    if(res > 0 && (readPFD->revents & POLLIN)){
        return 1;
    }
    //-1 means error
    // 0 means system timed out
    return res;
}

uint32_t read_from_fifo(struct pollfd* readPFD, char* buffer, size_t arraySize, uint32_t msgLength){
    uint32_t totalRead = 0;
    while(totalRead < msgLength){
        ssize_t bytesRead = read(readPFD->fd, buffer + totalRead, arraySize - totalRead - 1);
        if(bytesRead == 0){
            warning.store(WARNING_BADREAD);
            return 0;
        }
        else if(bytesRead < 0){
            warning.store(WARNING_BADREAD);
            return -1;
        }
        totalRead += bytesRead;
    }
    if(totalRead < arraySize){
        buffer[totalRead] = '\0';
    }

    return totalRead;
}

int write_to_fifo(struct pollfd* writePFD, char* buffer, size_t arraySize, uint32_t msgLength){
    size_t totalWritten = 0;
    while(totalWritten < msgLength){
        int written = write(writePFD->fd, buffer + totalWritten, msgLength - totalWritten);
        if(written == -1 || written == 0){
            return -1;
        }
        totalWritten += written;
    }
    return 1;
}

void fifo_error(struct pollfd* readPFD, struct pollfd* writePFD, int errorCode){
    close_fifo(readPFD, writePFD);
    if(FIFO_status.load() != SHUTDOWNCODE){
        FIFO_status.store(errorCode);
    }
}

void fifo_job(){
    //clean up any semblance of a FIFO 
    // that may have existed from before
    // (perhaps due to a sudden close/crash)
    if(!cleanup_fifo()){
        //should be fine but something should be notified
        warning.store(WARNING_BADCLEAN);
    }

    if(create_fifo() < 0){
        warning.store(WARNING_BADCREATION);
        return;
    }

    uint32_t msgLength;
    char msgLengthPtr[5];
    
    char local_buffer[Megabyte+1];
    std::memset(local_buffer, 0, Megabyte+1);
    struct pollfd readPFD;
    readPFD.fd = -1;            //must be set to -1 for init
    struct pollfd writePFD;
    writePFD.fd = -1;           //must be set to -1 for init

    FIFO_status.store(FIFOSTATUS_INIT);

    while(FIFO_status.load() != SHUTDOWNCODE){
        if(FIFO_hard_reset.load()){
            FIFO_hard_reset.store(false);
            fifo_error(&readPFD, &writePFD, FIFOSTATUS_UNEXPECTEDCLOSE);
        }
        int fifostatus = FIFO_status.load();
        if(paused.load()){continue;}
        if(fifostatus < 0 ||
           fifostatus == FIFOSTATUS_INIT){

            int res = open_fifo(&readPFD, &writePFD);
            if(res == -1){
                warning.store(WARNING_FIFOBADOPEN);
                fifo_error(&readPFD, &writePFD, FIFOSTATUS_CLOSED);
                continue;
            }
            else if(res == 0){
                fifo_error(&readPFD, &writePFD, FIFOSTATUS_NOCLIENT);
                continue;
            }

            FIFO_status.store(FIFOSTATUS_LISTENING);
        }
        else if(fifostatus == FIFOSTATUS_LISTENING){
            int res = poll_fifo(&readPFD, 1000);
            if(res == -1){
                fifo_error(&readPFD, &writePFD, FIFOSTATUS_BADPOLL);
                continue;
            }
            else if(res == 0){
                warning.store(WARNING_POLLTIMEOUT);
                continue;
            }

            //for debugging
            //uint32_t fortest = read_from_fifo(&readPFD, local_buffer, Megabyte, Megabyte);

            
            uint32_t helpme = read_from_fifo(&readPFD, msgLengthPtr, sizeof(msgLengthPtr), 4);
            msgLength = readInteger(msgLengthPtr);

            res = poll_fifo(&readPFD, 1000);
            if(res == -1){
                fifo_error(&readPFD, &writePFD, FIFOSTATUS_BADPOLL);
                continue;
            }
            else if(res == 0){
                warning.store(WARNING_POLLTIMEOUT);
                continue;
            }
            uint32_t readLengthU = read_from_fifo(&readPFD, local_buffer, sizeof(local_buffer), msgLength);
            int readLength = (int) readLengthU;
            
            if(readLength > 0){
                //sendToFIFO(local_buffer); // for debugging
            }
            
            if((readLength != 4 || !strcomp(local_buffer, "PING", msgLength))){
                fifo_error(&readPFD, &writePFD, FIFOSTATUS_CLOSED);
                warning.store(WARNING_BADMESSAGE);
                continue;
            }

            writeInteger(msgLengthPtr, 4);
            res = write_to_fifo(&writePFD, msgLengthPtr, sizeof(msgLengthPtr), 4);
            if(res == 0){
                fifo_error(&readPFD, &writePFD, FIFOSTATUS_CLOSED);
                warning.store(WARNING_BADMESSAGE);
                continue;
            }

            
            std::strcpy(local_buffer, "PONG");
            res = write_to_fifo(&writePFD, local_buffer, sizeof(local_buffer), msgLength);
            if(res == 0){
                fifo_error(&readPFD, &writePFD, FIFOSTATUS_CLOSED);
                warning.store(WARNING_BADMESSAGE);
                continue;
            }

            FIFO_status.store(FIFOSTATUS_CONNECTED);
        }
        else if(fifostatus == FIFOSTATUS_CONNECTED){
            int res = poll_fifo(&readPFD, 1000);
            if(res == -1){
                fifo_error(&readPFD, &writePFD, FIFOSTATUS_BADPOLL);
                continue;
            }
            else if(res == 0){
                continue;
            }

            res = read_from_fifo(&readPFD, msgLengthPtr, sizeof(msgLengthPtr), 4);
            msgLength = readInteger(msgLengthPtr);
            int val = (int) res;


            res = poll_fifo(&readPFD, 1000);
            if(res == -1){
                fifo_error(&readPFD, &writePFD, FIFOSTATUS_BADPOLL);
                continue;
            }
            else if(res == 0){
                warning.store(WARNING_POLLTIMEOUT);
                continue;
            }
            uint32_t readLengthU = read_from_fifo(&readPFD, local_buffer, sizeof(local_buffer), msgLength);
            int readLength = (int) readLengthU;
            if(readLength <= 0){
                //TODO
                //verify this?
                continue;
            }
            sendToSocket(local_buffer);

            //NOTE
            //This hangs. 
            //I'm not sure if I like this implementation because 
            // if two users send a message, we have to separate 
            // the request into each second.
            //  - I mean, we could try to shorten the duration
            //      but do we want each request to wait for its
            //      previous one?
            //     - We can always implement a queue system
            //      where if multiple users send a request within the 
            //      duration, we can add any non-processing sends 
            //      to a queue. There's no way someone in our company
            //      will overload the queue with 1000s of requests.
            //      and pollBuffer should wait a short enough time before 
            //      quitting the query entirely
            res = pollBuffer(TOFIFO, 1000);
            if(res == 0){
                std::strcpy(local_buffer, "ACK:SOCKET DID NOT RESPOND IN TIME");
                msgLength = std::strlen(local_buffer);
                writeInteger(msgLengthPtr, msgLength);
                res = write_to_fifo(&writePFD, msgLengthPtr, sizeof(msgLengthPtr), 4);
                //TODO
                //should we verify res?
                
                res = write_to_fifo(&writePFD, local_buffer, sizeof(local_buffer), msgLength);
                //TODO 
                //should we verify res?

                continue;
            }
            
            res = readFromSocket(local_buffer);
            if(res == 0){
                //How did we get here?
            }

            msgLength = std::strlen(local_buffer);
            writeInteger(msgLengthPtr, msgLength);
            res = write_to_fifo(&writePFD, msgLengthPtr, sizeof(msgLengthPtr), 4);
            //TODO
            //should we verify res?
            
            res = write_to_fifo(&writePFD, local_buffer, sizeof(local_buffer), msgLength);
            //TODO should we verify res?

            //This old version made the fifo thread handle
            // different query scenarios
            //This prevents the whole purpose of main loop
            // which is to handle the communcation between the socket 
            // and the FIFO
            /*
            if((msgLength > 7 && strcomp(local_buffer, "search ", 7)) ||
                (msgLength > 9 && strcomp(local_buffer, "analysis ", 9))){
                    sendToSocket(local_buffer);
            }
            else if(msgLength == 6 && strcomp(local_buffer, "online", 6)){
                if(socket_state.load() == SOCKET_CONNECTED){
                    std::strcpy(local_buffer, "RESULT:\nSOCKET ONLINE");
                }
                else{
                    std::strcpy(local_buffer, "RESULT:\nSOCKET OFFLINE");
                }

                msgLength = std::strlen(local_buffer);
                writeInteger(msgLengthPtr, msgLength);
                res = write_to_fifo(&writePFD, msgLengthPtr, sizeof(msgLengthPtr), 4);
                //TODO
                //check the res value

                res = write_to_fifo(&writePFD, local_buffer, sizeof(local_buffer), msgLength);
                //TODO
                //check the res value
                continue;
            }
            else{
                std::strcpy(local_buffer, "RESULT:\nBAD_QUERY");

                msgLength = std::strlen(local_buffer);
                writeInteger(msgLengthPtr, msgLength);
                res = write_to_fifo(&writePFD, msgLengthPtr, sizeof(msgLengthPtr), 4);
                //TODO
                //check the res value

                res = write_to_fifo(&writePFD, local_buffer, sizeof(local_buffer), msgLength);
                //TODO
                //check the res value

                warning.store(WARNING_BADQUERY);
                continue;
            }

            FIFO_status.store(FIFOSTATUS_BUSY);
            */
        }
        else if(fifostatus == FIFOSTATUS_BUSY){
            sendToSocket("progress");

            int gotResponse = 0;
            //try 1000 times
            while(!gotResponse){
                int res = readFromSocket(local_buffer);
                if(!res){
                    continue;
                }

                msgLength = std::strlen(local_buffer);
                writeInteger(msgLengthPtr, msgLength);
                res = write_to_fifo(&writePFD, msgLengthPtr, sizeof(msgLengthPtr), 4);
                //TODO
                //check res

                res = write_to_fifo(&writePFD, local_buffer, sizeof(local_buffer), msgLength);
                //tODO
                //check res
                gotResponse = 1;
            }

            if(strcomp(local_buffer, "RESULT", 6)){
                FIFO_status.store(FIFOSTATUS_CONNECTED);
            }
        }
        else{
            warning.store(WARNING_HOW);
        }
    }

    unlink(FIFOinputpath.c_str());
    unlink(FIFOoutputpath.c_str());
}