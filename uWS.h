#ifndef UWS_H
#define UWS_H

#include <cstddef>
#include <functional>

namespace uWS {

class Socket {
    friend class Server;
private:
    void *socket;
    Socket(void *p) : socket(p)
    {}
public:

    // this should be made more capable, with char flags!
    void send(char *data, size_t length, bool binary);

    // this function is just a helper,
    // not very good because you need to know the full length up front!
    void sendFragment(char *data, size_t length, bool binary, size_t remainingBytes);
};

class Server
{
private:
    static void onReadable(void *vp, int status, int events);
    static void onWritable(void *vp, int status, int events);
    static void onAcceptable(void *vp, int status, int events);
    void (*connectionCallback)(Socket);
    void (*disconnectionCallback)(Socket);
    void (*fragmentCallback)(Socket, const char *, size_t, bool, size_t);
    static const int BUFFER_SIZE = 1024 * 300;
    char *receiveBuffer;

public:
    Server(int port);
    ~Server();
    void onConnection(void (*connectionCallback)(Socket));
    void onDisconnection(void (*disconnectionCallback)(Socket));
    void onFragment(void (*fragmentCallback)(Socket, const char *, size_t, bool, size_t));
    void send(void *vp, char *data, size_t length, bool binary, int flags);
    void run();
};

}

#endif // UWS_H
