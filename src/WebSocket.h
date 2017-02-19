#ifndef WEBSOCKET_UWS_H
#define WEBSOCKET_UWS_H

#include "WebSocketProtocol.h"
#include "Socket.h"

namespace uWS {

template <bool isServer>
struct Group;

template <const bool isServer>
struct WIN32_EXPORT WebSocket : protected uS::Socket {
    struct Data : uS::SocketData, WebSocketProtocol<isServer> {
        std::string fragmentBuffer, controlBuffer;
        enum CompressionStatus : char {
            DISABLED,
            ENABLED,
            COMPRESSED_FRAME
        } compressionStatus;
        bool hasOutstandingPong = false;

        Data(bool perMessageDeflate, uS::SocketData *socketData) : uS::SocketData(*socketData) {
            compressionStatus = perMessageDeflate ? CompressionStatus::ENABLED : CompressionStatus::DISABLED;
        }
    };

    WebSocket(uS::Socket s = nullptr) : uS::Socket(s) {

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
        ((Group<isServer> *) getSocketData()->nodeData)->removeWebSocket(p);
        uS::Socket::transfer((uS::NodeData *) group, [](uv_poll_t *p) {
            uS::Socket s(p);
            ((Group<isServer> *) s.getSocketData()->nodeData)->addWebSocket(s);
        });
    }

    uv_poll_t *getPollHandle() const {return p;}
    void terminate();
    void close(int code = 1000, const char *message = nullptr, size_t length = 0);
    void ping(const char *message) {send(message, OpCode::PING);}
    void send(const char *message, OpCode opCode = OpCode::TEXT) {send(message, strlen(message), opCode);}
    void send(const char *message, size_t length, OpCode opCode, void(*callback)(void *webSocket, void *data, bool cancelled, void *reserved) = nullptr, void *callbackData = nullptr);
    static PreparedMessage *prepareMessage(char *data, size_t length, OpCode opCode, bool compressed, void(*callback)(void *webSocket, void *data, bool cancelled, void *reserved) = nullptr);
    static PreparedMessage *prepareMessageBatch(std::vector<std::string> &messages, std::vector<int> &excludedMessages, OpCode opCode, bool compressed, void(*callback)(void *webSocket, void *data, bool cancelled, void *reserved) = nullptr);
    void sendPrepared(PreparedMessage *preparedMessage, void *callbackData = nullptr);
    static void finalizeMessage(PreparedMessage *preparedMessage);
    bool operator==(const WebSocket &other) const {return p == other.p;}
    bool operator<(const WebSocket &other) const {return p < other.p;}

private:
    friend class uS::Socket;
    template <bool> friend struct Group;
    static void onData(uS::Socket s, char *data, int length);
    static void onEnd(uS::Socket s);
};

}

namespace std {

template <bool isServer>
struct hash<uWS::WebSocket<isServer>> {
    std::size_t operator()(const uWS::WebSocket<isServer> &webSocket) const
    {
        return std::hash<uv_poll_t *>()(webSocket.getPollHandle());
    }
};

}

#endif // WEBSOCKET_UWS_H
