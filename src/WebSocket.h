#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include "WebSocketData.h"

namespace uWS {

template <bool SSL>
struct WebSocket {
private:

public:
    void init() {
        // construct us

        new (us_socket_ext((us_socket *) this)) WebSocketData;
    }
};

}

#endif // WEBSOCKET_H
