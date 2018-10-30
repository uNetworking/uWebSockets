#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include "WebSocketData.h"
#include "WebSocketProtocol.h"
#include "AsyncSocket.h"

#include <string_view>

namespace uWS {

template <bool SSL, bool isServer>
struct WebSocket : AsyncSocket<SSL> {
private:
    typedef AsyncSocket<SSL> Super;
public:

    void send(std::string_view message) {

        // if corkAllocate(size) then corkFree(unused)

        // format the response
        char buf[100];
        int writeLength = WebSocketProtocol<isServer, WebSocket<SSL, isServer>>::formatMessage(buf, message.data(), message.length(), uWS::OpCode::TEXT, message.length(), false);



        Super::write(buf, writeLength);

    }

    // absolutely not public!
    void init() {
        // construct us

        new (us_socket_ext((us_socket *) this)) WebSocketData;
    }
};

}

#endif // WEBSOCKET_H
