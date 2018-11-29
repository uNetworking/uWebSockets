/*
 * Copyright 2018 Alex Hultman and contributors.

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 *     http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

    void *init(bool perMessageDeflate) {
        new (us_socket_ext((us_socket *) this)) WebSocketData(perMessageDeflate);
        return this;
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
