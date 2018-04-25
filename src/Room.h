#ifndef ROOM_H
#define ROOM_H

// Room is used to implement efficient broadcasting or pub/sub
// As opposed to Group, Room is _efficient_ for all broadcasting problems

#include "WebSocket.h"
#include <vector>

namespace uS {
struct Loop;
}

namespace uWS {

template <bool isServer>
class Room {
private:
    std::vector<uWS::WebSocket<isServer> *> webSockets;

    void flush();
public:
    Room(uS::Loop *loop);
    void add(WebSocket<isServer> *ws);
    void remove(WebSocket<isServer> *ws);

    // todo: decide how it looks!
    void send(const char *message, size_t length, OpCode opCode, WebSocket<isServer> *excludedSender = nullptr);
};

}

#endif // ROOM_H
