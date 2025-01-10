#pragma once

#include <string>
#include <mutex>
#include <atomic>

extern std::atomic<bool> paused;

#define Megabyte (1024 * 1024)
#define TOSOCKET 1
#define TOFIFO   2

extern std::mutex mtx;
extern std::atomic<int> bufferDirection;
extern std::atomic<bool> bufferChanged;
extern char buffer[];

extern std::atomic<int> warning;
#define WARNING_NOTHINGREAD    -112
#define WARNING_DISTRUSTSOCK   -111
#define WARNING_BADPINGRECV    -110
#define WARNING_BADLISTEN      -109
#define WARNING_NOPORT         -108
#define WARNING_GETTINGPROG    -107
#define WARNING_BADREADPROG    -106
#define WARNING_BADSENDQUERY   -105
#define WARNING_NOCONNECT      -104
#define WARNING_BADOPEN        -103
#define WARNING_BADPINGSEND    -102
#define WARNING_HOWSOCKET      -101
#define WARNING_BADMESSAGE       -6
#define WARNING_FIFOBADOPEN      -5
#define WARNING_HOW              -4
#define WARNING_BADDIRECTION     -3
#define WARNING_BADREAD          -2
#define WARNING_BADCREATION      -1
#define WARNING_BADCLEAN          1
#define WARNING_BADQUERY          2
#define WARNING_POLLTIMEOUT       3

void writeToBuffer(const char* str);
void readFromBuffer(char* res);

void sendToSocket(const char* str);
int readFromSocket(char* str);

void sendToFIFO(const char* str);
int readFromFIFO(char* res);

std::string interpret_warning(int value);
int strcomp(const char* a, const char* b, uint32_t length);

