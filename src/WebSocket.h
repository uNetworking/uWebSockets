#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include "WebSocketData.h"
#include "WebSocketProtocol.h"
#include "AsyncSocket.h"

#include <string_view>

namespace uWS {

template <bool SSL, bool isServer>
struct WebSocket : AsyncSocket<SSL> {
    template <bool> friend struct TemplatedApp;
private:
    typedef AsyncSocket<SSL> Super;

    void init() {
        new (us_socket_ext((us_socket *) this)) WebSocketData;
    }
public:

    // this function need clean-ups and perf. fixes
    void send(std::string_view message, uWS::OpCode opCode) {

        // if corkAllocate(size) then corkFree(unused)

        // format the response
        char *buf = (char *) malloc(message.length() + 100);
        int writeLength = WebSocketProtocol<isServer, WebSocket<SSL, isServer>>::formatMessage(buf, message.data(), message.length(), opCode, message.length(), false);



        Super::write(buf, writeLength);

        free(buf);

    }

    /* Emit close event, stat passive timeout */
    void close(int code, std::string_view message = {}) {
        // closing should trigger close event!

        std::cout << "Closing websocket: " << code << " = " << message << std::endl;

        static const int MAX_CLOSE_PAYLOAD = 123;
        int length = std::min<size_t>(MAX_CLOSE_PAYLOAD, message.length());

        // here we start a timeout and handle it accordingly in the timeout handler

        WebSocketData *webSocketData = (WebSocketData *) us_socket_ext((us_socket *) this);

        webSocketData->isShuttingDown = true;

        /* Format and send the close frame */
        char closePayload[MAX_CLOSE_PAYLOAD + 2];
        int closePayloadLength = (int) WebSocketProtocol<isServer, WebSocketContext<SSL, isServer>>::formatClosePayload(closePayload, code, message.data(), length);
        send(std::string_view(closePayload, closePayloadLength), OpCode::CLOSE);

        // why should we fin here?
        //us_socket_shutdown((us_socket *) this);
    }
};

}

#endif // WEBSOCKET_H
