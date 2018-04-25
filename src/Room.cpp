#include "Room.h"

namespace uWS {

template <bool isServer>
Room<isServer>::Room(uS::Loop *loop) {

    // we need to hook into the loop's post and pre-callbacks
    // every loop can have many rooms
    // Hub::createRoom() would solve that part
}

template <bool isServer>
void Room<isServer>::add(WebSocket<isServer> *ws) {
    // simple sorted vector add for now
}

template <bool isServer>
void Room<isServer>::remove(WebSocket<isServer> *ws) {
    // simple vector remove
}

template <bool isServer>
void Room<isServer>::flush() {

}

template <bool isServer>
void Room<isServer>::send(const char *message, size_t length, OpCode opCode, WebSocket<isServer> *excludedSender) {

}

template class Room<uWS::SERVER>;
template class Room<uWS::CLIENT>;

}
