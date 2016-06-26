#ifndef SERVER_H
#define SERVER_H

#include <mutex>
#include <queue>
#include <string>
#include <functional>
#include <uv.h>

#include "WebSocket.h"

namespace uWS {

enum Error {
    ERR_LISTEN
};

enum Options : int {
    NO_OPTIONS = 0,
    PERMESSAGE_DEFLATE = 1,
    SERVER_NO_CONTEXT_TAKEOVER = 2,
    CLIENT_NO_CONTEXT_TAKEOVER = 4,
    NO_DELAY = 8
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
    bool master, forceClose;
    int options, maxPayload;
    static void acceptHandler(uv_poll_t *p, int status, int events);
    static void upgradeHandler(Server *server);
    static void closeHandler(Server *server);

    char *recvBuffer, *sendBuffer, *inflateBuffer, *upgradeBuffer;
    static const int LARGE_BUFFER_SIZE = 307200,
                     SHORT_BUFFER_SIZE = 4096;

    struct UpgradeRequest {
        uv_os_fd_t fd;
        std::string sslKey;
        void *ssl;
        std::string extensions;
    };

    std::queue<UpgradeRequest> upgradeQueue;
    std::mutex upgradeQueueMutex;

    std::function<void(uv_os_fd_t, const char *, void *, const char *, size_t)> upgradeCallback;
    std::function<void(WebSocket)> connectionCallback;
    std::function<void(WebSocket, int code, char *message, size_t length)> disconnectionCallback;
    std::function<void(WebSocket, char *, size_t, OpCode)> messageCallback;
public:
    Server(int port = 0, bool master = true, int options = 0, int maxPayload = 1048576);
    ~Server();
    Server(const Server &server) = delete;
    Server &operator=(const Server &server) = delete;
    void onUpgrade(std::function<void(uv_os_fd_t, const char *, void *, const char *, size_t)> upgradeCallback);
    void onConnection(std::function<void(WebSocket)> connectionCallback);
    void onDisconnection(std::function<void(WebSocket, int code, char *message, size_t length)> disconnectionCallback);
    void onMessage(std::function<void(WebSocket, char *, size_t, OpCode)> messageCallback);
    void close(bool force = false);
    void upgrade(uv_os_fd_t fd, const char *secKey, void *ssl = nullptr, const char *extensions = nullptr, size_t extensionsLength = 0);
    void run();
    void broadcast(char *data, size_t length, OpCode opCode);
};

}

#endif // SERVER_H
