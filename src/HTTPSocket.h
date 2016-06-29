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

    uv_poll_t p;
    Server *server;
    void *ssl;

    HTTPSocket(uv_os_fd_t fd, Server *server, void *ssl);
    static void onReadable(uv_poll_t *p, int status, int events);
};

}

#endif // HTTPSOCKET_H
