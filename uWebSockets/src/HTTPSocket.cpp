#include "HTTPSocket.h"
#include "Server.h"
#include "Network.h"

#include <iostream>
#include <utility>
#include <cstring>
#include <uv.h>
#include <openssl/ssl.h>

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

HTTPSocket::HTTPSocket(uv_poll_t *p, Server *server, void *ssl) : p(p), server(server), ssl(ssl)
{
    uv_poll_start(p, UV_READABLE, onReadable);
    p->data = this;

    t = new uv_timer_t;
    uv_timer_init(server->loop, t);
    uv_timer_start(t, onTimeout, 15000, 0);
    t->data = this;
}

uv_os_sock_t HTTPSocket::stop()
{
    uv_os_sock_t fd;
    uv_fileno((uv_handle_t *) p, (uv_os_fd_t *) &fd);

    uv_poll_stop(p);
    uv_close((uv_handle_t *) p, [](uv_handle_t *handle) {
        delete (uv_poll_t *) handle;
    });

    uv_timer_stop(t);
    uv_close((uv_handle_t *) t, [](uv_handle_t *handle) {
        delete (uv_timer_t *) handle;
    });

    return fd;
}

void HTTPSocket::close(uv_os_sock_t fd)
{
    if (ssl) {
        SSL_free((SSL *) ssl);
    }
    ::close(fd);
}

void HTTPSocket::onTimeout(uv_timer_t *t)
{
    HTTPSocket *httpData = (HTTPSocket *) t->data;
    httpData->close(httpData->stop());
    delete httpData;
}

void HTTPSocket::onReadable(uv_poll_t *p, int status, int events)
{
    HTTPSocket *httpData = (HTTPSocket *) p->data;

    if (status < 0) {
        httpData->close(httpData->stop());
        delete httpData;
        return;
    }

    uv_os_sock_t fd;
    uv_fileno((uv_handle_t *) p, (uv_os_fd_t *) &fd);

    int length;
    if (httpData->ssl) {
        length = SSL_read((SSL *) httpData->ssl, httpData->server->recvBuffer, Server::LARGE_BUFFER_SIZE);
        if (length < 1) {
            switch (SSL_get_error((SSL *) httpData->ssl, length)) {
            case SSL_ERROR_WANT_WRITE:
            case SSL_ERROR_WANT_READ:
                return;
            }
        }
    } else {
        length = recv(fd, httpData->server->recvBuffer, Server::LARGE_BUFFER_SIZE, 0);
    }

    if (length == SOCKET_ERROR || length == 0 || httpData->headerBuffer.length() + length > MAX_HEADER_BUFFER_LENGTH) {
        int fd = httpData->stop();
        httpData->close(fd);
        delete httpData;
        return;
    }

    httpData->headerBuffer.append(httpData->server->recvBuffer, length);
    if (httpData->headerBuffer.find("\r\n\r\n") != std::string::npos) {

        // stop poll and timer
        uv_os_sock_t fd = httpData->stop();

        // parse secKey, extensions
        Request h = (char *) httpData->headerBuffer.data();
        std::pair<char *, size_t> secKey = {}, extensions = {};
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
                httpData->server->upgradeCallback(fd, secKey.first, httpData->ssl, extensions.first, extensions.second);
            } else {
                httpData->server->upgrade(fd, secKey.first, httpData->ssl, extensions.first, extensions.second);
            }
        } else {
            // we do not handle any HTTP-only requests
            httpData->close(fd);
        }
        delete httpData;
    }
}

}
