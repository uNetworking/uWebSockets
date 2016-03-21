#ifndef SERVER_H
#define SERVER_H

#include <cstddef>
#include <sys/socket.h>
#include <netinet/in.h>

class Server
{
private:
    static void onReadable(void *vp, int status, int events);
    static void onWritable(void *vp, int status, int events);
    static void onAcceptable(void *vp, int status, int events);

public:
    Server(int port);
    static void onFragment(void *vp, char *fragment, size_t length);
    void send(void *vp, char *data, size_t length);
    void run();
};

#endif // SERVER_H
