#ifndef GROUP_H
#define GROUP_H

#include "WebSocket.h"
#include "Extensions.h"
#include <functional>

namespace uWS {

struct Hub;

struct UpgradeInfo {
    char *path, *subprotocol;
    size_t pathLength, subprotocolLength;
};

template <bool isServer>
struct Group : protected uS::NodeData {
    friend class Hub;
    std::function<void(WebSocket<isServer>, UpgradeInfo)> connectionHandler;
    std::function<void(WebSocket<isServer>, char *message, size_t length, OpCode opCode)> messageHandler;
    std::function<void(WebSocket<isServer>, int code, char *message, size_t length)> disconnectionHandler;
    std::function<void(WebSocket<isServer>, char *, size_t)> pingHandler;
    std::function<void(WebSocket<isServer>, char *, size_t)> pongHandler;

    using errorType = typename std::conditional<isServer, int, void *>::type;
    std::function<void(errorType)> errorHandler;

    Hub *hub;
    int extensionOptions;

    // todo: cannot be named user, collides with parent!
    void *userData = nullptr;
    void setUserData(void *user);
    void *getUserData();

    uv_poll_t *webSocketHead = nullptr, *httpSocketHead = nullptr;
    void addWebSocket(uv_poll_t *webSocket);
    void removeWebSocket(uv_poll_t *webSocket);

    struct WebSocketIterator {
        uv_poll_t *webSocket;
        WebSocketIterator(uv_poll_t *webSocket) : webSocket(webSocket) {}
        WebSocket<isServer> operator*() {
            return WebSocket<isServer>(webSocket);
        }

        bool operator!=(const WebSocketIterator &other) {
            return !(webSocket == other.webSocket);
        }

        WebSocketIterator &operator++() {
            uS::SocketData *socketData = (uS::SocketData *) webSocket->data;
            webSocket = socketData->next;
            return *this;
        }
    };

protected:
    Group(int extensionOptions, Hub *hub, uS::NodeData *nodeData);
    void stopListening();

public:
    void onConnection(std::function<void(WebSocket<isServer>, UpgradeInfo ui)> handler);
    void onMessage(std::function<void(WebSocket<isServer>, char *, size_t, OpCode)> handler);
    void onDisconnection(std::function<void(WebSocket<isServer>, int code, char *message, size_t length)> handler);
    void onPing(std::function<void(WebSocket<isServer>, char *, size_t)> handler);
    void onPong(std::function<void(WebSocket<isServer>, char *, size_t)> handler);
    void onError(std::function<void(errorType)> handler);

    void broadcast(const char *message, size_t length, OpCode opCode);
    void terminate();
    void close(int code = 1000, char *message = nullptr, size_t length = 0);
    using NodeData::addAsync;

    WebSocketIterator begin() {
        return WebSocketIterator(webSocketHead);
    }

    WebSocketIterator end() {
        return WebSocketIterator(nullptr);
    }
};

}

#endif // GROUP_H
