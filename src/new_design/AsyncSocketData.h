#ifndef ASYNCSOCKETDATA_H
#define ASYNCSOCKETDATA_H

#include <string>

// todo: think about chains of AsyncSocketData too!
// we want to buffer things up in one buffer, or in many separate ones (like with websockets)

template <bool SSL>
struct AsyncSocketData {

    // we need a buffer
    std::string buffer;

};

#endif // ASYNCSOCKETDATA_H
