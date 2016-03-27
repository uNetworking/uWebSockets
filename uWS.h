#ifndef UWS_H
#define UWS_H

#include <cstddef>
#include <functional>
#include <string>

namespace uWS {

enum OpCode : unsigned char {
    TEXT = 1,
    BINARY = 2,
    PING = 9,
    PONG = 10
};

class Parser;

class Socket {
    friend class Server;
    friend class Parser;
private:
    void *socket;
    Socket(void *p) : socket(p) {}
    void write(char *data, size_t length, bool transferOwnership);

    static const int SHORT_SEND = 4096;
    static char *sendBuffer;
public:
    void fail();
    void send(char *data, size_t length, OpCode opCode);
    void sendFragment(char *data, size_t length, bool binary, size_t remainingBytes);
};

class Server
{
    friend class Parser;
    friend class Socket;
private:
    // internal callbacks
    static void onReadable(void *vp, int status, int events);
    static void onWritable(void *vp, int status, int events);
    static void onAcceptable(void *vp, int status, int events);

    // external callbacks
    void (*connectionCallback)(Socket);
    void (*disconnectionCallback)(Socket);
    void (*fragmentCallback)(Socket, const char *, size_t, OpCode, bool, size_t);

    // buffers
    static const int BUFFER_SIZE = 1024 * 300;
    char *receiveBuffer;

    // socket management
    void disconnect(void *vp);
    void upgrade(int fd, const char *secKey);

public:
    Server(int port);
    ~Server();
    void onConnection(void (*connectionCallback)(Socket));
    void onDisconnection(void (*disconnectionCallback)(Socket));
    void onFragment(void (*fragmentCallback)(Socket, const char *, size_t, OpCode, bool, size_t));
    void run();
    static bool isValidUtf8(std::string &str);
};

}

#endif // UWS_H
