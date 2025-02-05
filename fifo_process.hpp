#pragma once

//POSIX stuff: shows an error on visual studio code
#include <unistd.h>             // For write, read, close
#include <sys/poll.h>           // For poll

#include <atomic>

#include <cstdlib>
#include <cstring>
#include <fcntl.h>              // For open, O_WRONLY
#include <sys/stat.h>           // For mkfifo

#include <string>
#include <filesystem>           // for C++17
namespace fs = std::filesystem;
extern const std::string FIFOdirpath;
extern const std::string FIFOinputpath;
extern const std::string FIFOoutputpath;


extern std::atomic<int> FIFO_status;
extern std::atomic<bool> FIFO_hard_reset;
#define SHUTDOWNCODE                     -10

#define FIFOSTATUS_INIT                    0                 
#define FIFOSTATUS_LISTENING               1
#define FIFOSTATUS_CONNECTED               2
#define FIFOSTATUS_BUSY                    3
#define FIFOSTATUS_BADPOLL                -1
#define FIFOSTATUS_CLOSED                 -2
#define FIFOSTATUS_BADCONNECT             -3
#define FIFOSTATUS_NOCLIENT               -4
#define FIFOSTATUS_UNEXPECTEDCLOSE        -5

//string functions

int cleanup_fifo();

int create_fifo();

void close_fifo(struct pollfd* readPFD, struct pollfd* writePFD);

int open_fifo(struct pollfd* readPFD, struct pollfd* writePFD);

int poll_fifo(struct pollfd* readPFD, int timeout);

uint32_t read_from_fifo(struct pollfd* readPFD, char* buffer, size_t arraySize, uint32_t msgLength);

int write_to_fifo(struct pollfd* writePFD, char* buffer, size_t arraySize, uint32_t msgLength);

void fifo_error(struct pollfd* readPFD, struct pollfd* writePFD, int errorCode);

void fifo_job();