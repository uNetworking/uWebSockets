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

    void send(std::string_view message, uWS::OpCode opCode) {

        // if corkAllocate(size) then corkFree(unused)

        // format the response
        char *buf = (char *) malloc(message.length() + 100);
        int writeLength = WebSocketProtocol<isServer, WebSocket<SSL, isServer>>::formatMessage(buf, message.data(), message.length(), opCode, message.length(), false);



        Super::write(buf, writeLength);

        free(buf);

    }

    // absolutely not public!
    void init() {
        // construct us

        new (us_socket_ext((us_socket *) this)) WebSocketData;
    }
};

}

#endif // WEBSOCKET_H
