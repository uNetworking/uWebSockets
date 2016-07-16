#ifndef HTTPSOCKET_H
#define HTTPSOCKET_H

#include <string>
#include <vector>
#include <uv.h>

namespace uWS {

class Server;

class HTTPSocket {
    friend class Server;
    std::string headerBuffer;
    std::vector<std::pair<char *, size_t>> headers;
    static const int MAX_HEADER_BUFFER_LENGTH = 10240;

    uv_poll_t *p;
    uv_timer_t *t;
    Server *server;
    void *ssl;

    HTTPSocket(uv_poll_t *p, Server *server, void *ssl);
    uv_os_sock_t stop();
    void close(uv_os_sock_t fd);
    static void onReadable(uv_poll_t *p, int status, int events);
    static void onTimeout(uv_timer_t *t);
};

}

#endif // HTTPSOCKET_H
