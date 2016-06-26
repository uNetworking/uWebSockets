/* The native HTTP support is currently very basic and somewhat unfinished */

#include "HTTPSocket.h"
#include "Server.h"
#include "Network.h"

#include <iostream>
#include <utility>
#include <cstring>
#include <uv.h>

struct Request {
    char *cursor;
    std::pair<char *, size_t> key, value;
    Request(char *cursor) : cursor(cursor)
    {
        size_t length;
        for (; isspace(*cursor); cursor++);
        for (length = 0; !isspace(cursor[length]) && cursor[length] != '\r'; length++);
        key = {cursor, length};
        cursor += length + 1;
        for (length = 0; !isspace(cursor[length]) && cursor[length] != '\r'; length++);
        value = {cursor, length};
    }
    Request &operator++(int)
    {
        size_t length = 0;
        for (; !(cursor[0] == '\r' && cursor[1] == '\n'); cursor++);
        cursor += 2;
        if (cursor[0] == '\r' && cursor[1] == '\n') {
            key = value = {0, 0};
        } else {
            for (; cursor[length] != ':' && cursor[length] != '\r'; length++);
            key = {cursor, length};
            if (cursor[length] != '\r') {
                cursor += length;
                length = 0;
                while (isspace(*(++cursor)));
                for (; cursor[length] != '\r'; length++);
                value = {cursor, length};
            } else {
                value = {0, 0};
            }
        }
        return *this;
    }
};

namespace uWS {

HTTPSocket::HTTPSocket(uv_os_fd_t fd, Server *server) : server(server)
{
    uv_poll_init_socket(server->loop, &p, fd);
    uv_poll_start(&p, UV_READABLE, onReadable);
    p.data = this;
}

void HTTPSocket::onReadable(uv_poll_t *p, int status, int events)
{
    if (status < 0) {
        // error read
    }

    uv_os_fd_t fd;
    uv_fileno((uv_handle_t *) p, &fd);

    HTTPSocket *httpData = (HTTPSocket *) p->data;
    int length = recv(fd, httpData->server->recvBuffer, Server::LARGE_BUFFER_SIZE, 0);
    httpData->headerBuffer.append(httpData->server->recvBuffer, length);

    // did we read the complete header?
    if (httpData->headerBuffer.find("\r\n\r\n") != std::string::npos) {

        // our part is done here
        uv_poll_stop(p);
        uv_close((uv_handle_t *) p, [](uv_handle_t *handle) {
            delete (HTTPSocket *) handle->data;
            //delete (uv_poll_t *) handle;
        });

        Request h = (char *) httpData->headerBuffer.data();

        // strip away any ? from the GET request
        h.value.first[h.value.second] = 0;
        for (size_t i = 0; i < h.value.second; i++) {
            if (h.value.first[i] == '?') {
                h.value.first[i] = 0;
                break;
            } else {
                // lowercase the request path
                h.value.first[i] = tolower(h.value.first[i]);
            }
        }

        std::pair<char *, size_t> secKey = {}, extensions = {};

        // only accept requests with our path
        //if (!strcmp(h.value.first, httpData->server->path.c_str())) {
            for (h++; h.key.second; h++) {
                if (h.key.second == 17 || h.key.second == 24) {
                    // lowercase the key
                    for (size_t i = 0; i < h.key.second; i++) {
                        h.key.first[i] = tolower(h.key.first[i]);
                    }
                    if (!strncmp(h.key.first, "sec-websocket-key", h.key.second)) {
                        secKey = h.value;
                    } else if (!strncmp(h.key.first, "sec-websocket-extensions", h.key.second)) {
                        extensions = h.value;
                    }
                }
            }

            // this is an upgrade
            if (secKey.first && secKey.second == 24) {
                if (httpData->server->upgradeCallback) {
                    httpData->server->upgradeCallback(fd, secKey.first, nullptr, extensions.first, extensions.second);
                } else {
                    httpData->server->upgrade(fd, secKey.first, nullptr, extensions.first, extensions.second);
                }
                return;
            }
        //}

        // for now, we just close HTTP traffic
        ::close(fd);
    } else {
        // todo: start timer to time out the connection!

    }
}

}
