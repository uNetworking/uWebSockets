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

class Server
{
    friend class HTTPSocket;
    friend class WebSocket;
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
        WebSocket webSocket;
        WebSocketIterator(WebSocket webSocket) : webSocket(webSocket) {

        }

        WebSocket &operator*() {
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

    std::function<void(uv_os_sock_t, const char *, void *, const char *, size_t)> upgradeCallback;
    std::function<void(WebSocket)> connectionCallback;
    std::function<void(WebSocket, int code, char *message, size_t length)> disconnectionCallback;
    std::function<void(WebSocket, char *, size_t, OpCode)> messageCallback;
    std::function<void(WebSocket, char *, size_t)> pingCallback;
    std::function<void(WebSocket, char *, size_t)> pongCallback;
public:
    Server(int port = 0, bool master = true, unsigned int options = 0, unsigned int maxPayload = 1048576, SSLContext sslContext = SSLContext());
    ~Server();
    Server(const Server &server) = delete;
    Server &operator=(const Server &server) = delete;
    void onUpgrade(std::function<void(uv_os_sock_t, const char *, void *, const char *, size_t)> upgradeCallback);
    void onConnection(std::function<void(WebSocket)> connectionCallback);
    void onDisconnection(std::function<void(WebSocket, int code, char *message, size_t length)> disconnectionCallback);
    void onMessage(std::function<void(WebSocket, char *, size_t, OpCode)> messageCallback);
    void onPing(std::function<void(WebSocket, char *, size_t)> pingCallback);
    void onPong(std::function<void(WebSocket, char *, size_t)> pongCallback);
    void close(bool force = false);
    void upgrade(uv_os_sock_t fd, const char *secKey, void *ssl = nullptr, const char *extensions = nullptr, size_t extensionsLength = 0);
    void run();
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
