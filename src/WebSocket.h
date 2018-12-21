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
    using SOCKET_TYPE = typename StaticDispatch<SSL>::SOCKET_TYPE;
    using StaticDispatch<SSL>::static_dispatch;

    void *init(bool perMessageDeflate) {
        new (static_dispatch(us_ssl_socket_ext, us_socket_ext)((SOCKET_TYPE *) this)) WebSocketData(perMessageDeflate);
        return this;
    }
public:

    /* Returns pointer to the per socket user data */
    //template <typename K>
    void *getUserData() {
        WebSocketData *webSocketData = (WebSocketData *) static_dispatch(us_ssl_socket_ext, us_socket_ext)((SOCKET_TYPE *) this);

        return nullptr;
    }

    /* See AsyncSocket */
    using Super::getBufferedAmount;

    /* Send or buffer a WebSocket frame, compressed or not. Returns false on increased user space backpressure. */
    bool send(std::string_view message, uWS::OpCode opCode = uWS::OpCode::BINARY, bool compress = false) {
        /* Transform the message to compressed domain if requested */
        if (compress) {
            //message = ;
            std::cout << "send compression ignored!" << std::endl;
        }

        /* Get size, alloate size, write if needed */
        size_t messageFrameSize = WebSocketProtocol<isServer, WebSocket<SSL, isServer>>::messageFrameSize(message.length());
        auto[sendBuffer, requiresWrite] = Super::getSendBuffer(messageFrameSize);
        WebSocketProtocol<isServer, WebSocket<SSL, isServer>>::formatMessage(sendBuffer, message.data(), message.length(), opCode, message.length(), false);
        if (requiresWrite) {
            auto[written, failed] = Super::write(sendBuffer, messageFrameSize);

            /* For now, we are slow here (fix!) */
            free(sendBuffer);

            /* Return true for success */
            return !failed;
        }

        /* Return success */
        return true;
    }

    /* Emit close event, stat passive timeout */
    void close(int code, std::string_view message = {}) {
        // closing should trigger close event!

        /*if (code == 1001) {
            std::cout << "Going away" << std::endl;
        }

        std::cout << "Closing websocket: " << code << " = " << message << std::endl;*/

        static const int MAX_CLOSE_PAYLOAD = 123;
        int length = std::min<size_t>(MAX_CLOSE_PAYLOAD, message.length());

        // here we start a timeout and handle it accordingly in the timeout handler

        //WebSocketData *webSocketData = (WebSocketData *) us_socket_ext((us_socket *) this);

        WebSocketData *webSocketData = (WebSocketData *) static_dispatch(us_ssl_socket_ext, us_socket_ext)((SOCKET_TYPE *) this);

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
