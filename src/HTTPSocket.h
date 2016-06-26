#ifndef HTTPSOCKET_H
#define HTTPSOCKET_H

#include <string>
#include <vector>
#include <uv.h>

namespace uWS {

class Server;

struct HTTPSocket {
    std::string headerBuffer;
    std::vector<std::pair<char *, size_t>> headers;

    uv_poll_t p;
    Server *server;

    HTTPSocket(uv_os_fd_t fd, Server *server);
    static void onReadable(uv_poll_t *p, int status, int events);
};

}

#endif // HTTPSOCKET_H
