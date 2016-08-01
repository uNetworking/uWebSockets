#ifndef SERVER_H
#define SERVER_H

#include <mutex>
#include <queue>
#include <string>
#include <functional>
#include <uv.h>
#include <openssl/ossl_typ.h>
#include <zlib.h>

#include "WebSocket.h"
#include "EventSystem.h"
#include "Agent.h"

namespace uWS {

enum Error {
    ERR_LISTEN,
    ERR_SSL,
    ERR_ZLIB
};

enum Options : unsigned int {
    NO_OPTIONS = 0,
    PERMESSAGE_DEFLATE = 1,
    SERVER_NO_CONTEXT_TAKEOVER = 2,
    CLIENT_NO_CONTEXT_TAKEOVER = 4,
    NO_DELAY = 8
};

class SSLContext {
private:
    SSL_CTX *sslContext = nullptr;
public:
    SSLContext(std::string certChainFileName, std::string keyFileName);
    SSLContext() = default;
    SSLContext(const SSLContext &other);
    ~SSLContext();
    operator bool() {
        return sslContext;
    }
    void *newSSL(int fd);
};

class Server : public Agent<WebSocket<SERVER>>
{
    friend class HTTPSocket;
    friend class WebSocket<SERVER>;
    //friend class WebSocket<CLIENT>;
private:
    uv_loop_t *loop;
    uv_poll_t *listenPoll = nullptr, *clients = nullptr;
    uv_async_t upgradeAsync, closeAsync;
    sockaddr_in listenAddr;
    z_stream writeStream;
    bool master, forceClose;
    unsigned int options, maxPayload;
    SSLContext sslContext;
    static void acceptHandler(uv_poll_t *p, int status, int events);
    static void upgradeHandler(Server *server);
    static void closeHandler(Server *server);

    char *recvBuffer, *sendBuffer, *inflateBuffer, *upgradeBuffer;
    static const int LARGE_BUFFER_SIZE = 307200,
                     SHORT_BUFFER_SIZE = 4096;

    struct WebSocketIterator {
        WebSocket<SERVER> webSocket;
        WebSocketIterator(WebSocket<SERVER> webSocket) : webSocket(webSocket) {

        }

        WebSocket<SERVER> &operator*() {
            return webSocket;
        }

        bool operator!=(const WebSocketIterator &other) {
            return !(webSocket == other.webSocket);
        }

        WebSocketIterator &operator++() {
            webSocket = webSocket.next();
            return *this;
        }
    };

    struct UpgradeRequest {
        uv_os_sock_t fd;
        std::string secKey;
        void *ssl;
        std::string extensions;
    };

    std::queue<UpgradeRequest> upgradeQueue;
    std::mutex upgradeQueueMutex;

public:
    Server(EventSystem &es, int port = 0, unsigned int options = 0, unsigned int maxPayload = 1048576, SSLContext sslContext = SSLContext());
    ~Server();
    Server(const Server &server) = delete;
    Server &operator=(const Server &server) = delete;
    void onUpgrade(std::function<void(uv_os_sock_t, const char *, void *, const char *, size_t)> upgradeCallback);
    void close(bool force = false);
    void upgrade(uv_os_sock_t fd, const char *secKey, void *ssl = nullptr, const char *extensions = nullptr, size_t extensionsLength = 0);
    size_t compress(char *src, size_t srcLength, char *dst);
    void broadcast(char *data, size_t length, OpCode opCode);

    WebSocketIterator begin() {
        return WebSocketIterator(clients);
    }

    WebSocketIterator end() {
        return WebSocketIterator(nullptr);
    }
};

}

#endif // SERVER_H
