#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include "WebSocketData.h"

#include "WebSocketProtocol.h"

#include <string_view>

namespace uWS {

template <bool SSL, bool isServer>
struct WebSocket {
private:

public:

    void send(std::string_view message) {

        // this path should use AsyncSocket with cork and everything

        // format the response
        char buf[100];
        int writeLength = WebSocketProtocol<isServer, WebSocket<SSL, isServer>>::formatMessage(buf, message.data(), message.length(), uWS::OpCode::TEXT, message.length(), false);


        us_socket_write((us_socket *) this, buf, writeLength, false);



    }

    // absolutely not public!
    void init() {
        // construct us

        new (us_socket_ext((us_socket *) this)) WebSocketData;
    }
};

}

#endif // WEBSOCKET_H
