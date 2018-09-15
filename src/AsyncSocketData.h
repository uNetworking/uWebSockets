#ifndef ASYNCSOCKETDATA_H
#define ASYNCSOCKETDATA_H

/* Depending on how we want AsyncSocket to function, this will need to change */

#include <string>

template <bool SSL>
struct AsyncSocketData {
    /* This will do for now */
    std::string buffer;
};

#endif // ASYNCSOCKETDATA_H
