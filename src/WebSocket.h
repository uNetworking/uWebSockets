#ifndef WEBSOCKET_UWS_H
#define WEBSOCKET_UWS_H

#include "WebSocketProtocol.h"
#include "Socket.h"

namespace uWS {

template <bool isServer>
struct Group;

template <const bool isServer>
struct WIN32_EXPORT WebSocket : uS::Socket, WebSocketProtocol<isServer> {
    enum CompressionStatus : char {
        DISABLED,
        ENABLED,
        COMPRESSED_FRAME
    } compressionStatus;
    char hasOutstandingPong = false;
    std::string fragmentBuffer, controlBuffer;

    WebSocket(bool perMessageDeflate, uS::Socket *socket) : uS::Socket(*socket) {
        compressionStatus = perMessageDeflate ? CompressionStatus::ENABLED : CompressionStatus::DISABLED;
    }

    struct PreparedMessage {
        char *buffer;
        size_t length;
        int references;
        void(*callback)(void *webSocket, void *data, bool cancelled, void *reserved);
    };

    using uS::Socket::getUserData;
    using uS::Socket::setUserData;
    using uS::Socket::getAddress;
    using uS::Socket::Address;

    void transfer(Group<isServer> *group) {
        ((Group<isServer> *) nodeData)->removeWebSocket(this);
        uS::Socket::transfer((uS::NodeData *) group, [](Poll *p) {
            uS::Socket *s = (uS::Socket *) p;
            ((Group<isServer> *) s->nodeData)->addWebSocket(s);
            ((Group<isServer> *) s->nodeData)->transferHandler((uWS::WebSocket<isServer> *) s);
        });
    }

    void terminate();
    void close(int code = 1000, const char *message = nullptr, size_t length = 0);
    void ping(const char *message) {send(message, OpCode::PING);}
    void send(const char *message, OpCode opCode = OpCode::TEXT) {send(message, strlen(message), opCode);}
    void send(const char *message, size_t length, OpCode opCode, void(*callback)(WebSocket<isServer> *webSocket, void *data, bool cancelled, void *reserved) = nullptr, void *callbackData = nullptr);
    static PreparedMessage *prepareMessage(char *data, size_t length, OpCode opCode, bool compressed, void(*callback)(WebSocket<isServer> *webSocket, void *data, bool cancelled, void *reserved) = nullptr);
    static PreparedMessage *prepareMessageBatch(std::vector<std::string> &messages, std::vector<int> &excludedMessages,
                                                OpCode opCode, bool compressed, void(*callback)(WebSocket<isServer> *webSocket, void *data, bool cancelled, void *reserved) = nullptr);
    void sendPrepared(PreparedMessage *preparedMessage, void *callbackData = nullptr);
    static void finalizeMessage(PreparedMessage *preparedMessage);

private:
    friend class uS::Socket;
    template <bool> friend struct Group;
    static uS::Socket *onData(uS::Socket *s, char *data, size_t length);
    static void onEnd(uS::Socket *s);
};

}

#endif // WEBSOCKET_UWS_H
