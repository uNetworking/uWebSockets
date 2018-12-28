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
    using SOCKET_CONTEXT_TYPE = typename StaticDispatch<SSL>::SOCKET_CONTEXT_TYPE;
    using StaticDispatch<SSL>::static_dispatch;

    void *init(bool perMessageDeflate, bool slidingCompression) {
        new (static_dispatch(us_ssl_socket_ext, us_socket_ext)((SOCKET_TYPE *) this)) WebSocketData(perMessageDeflate, slidingCompression);
        return this;
    }
public:

    /* Returns pointer to the per socket user data */
    void *getUserData() {
        WebSocketData *webSocketData = (WebSocketData *) static_dispatch(us_ssl_socket_ext, us_socket_ext)((SOCKET_TYPE *) this);
        /* We just have it overallocated by sizeof type */
        return (webSocketData + 1);
    }

    /* See AsyncSocket */
    using Super::getBufferedAmount;

    /* Send or buffer a WebSocket frame, compressed or not. Returns false on increased user space backpressure. */
    bool send(std::string_view message, uWS::OpCode opCode = uWS::OpCode::BINARY, bool compress = false) {
        /* Transform the message to compressed domain if requested */
        if (compress) {
            WebSocketData *webSocketData = (WebSocketData *) Super::getExt();

            /* Check and correct the compress hint */
            if (opCode < 3 && webSocketData->compressionStatus == WebSocketData::ENABLED) {
                LoopData *loopData = Super::getLoopData();
                /* Compress using either shared or dedicated deflationStream */
                if (webSocketData->deflationStream) {
                    message = webSocketData->deflationStream->deflate(loopData->zlibContext, message, false);
                } else {
                    message = loopData->deflationStream->deflate(loopData->zlibContext, message, true);
                }
            } else {
                compress = false;
            }
        }

        /* Get size, alloate size, write if needed */
        size_t messageFrameSize = protocol::messageFrameSize(message.length());
        auto[sendBuffer, requiresWrite] = Super::getSendBuffer(messageFrameSize);
        protocol::formatMessage<isServer>(sendBuffer, message.data(), message.length(), opCode, message.length(), compress);
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

    /* Emit close event, start passive timeout */
    void close(int code, std::string_view message = {}) {
        static const int MAX_CLOSE_PAYLOAD = 123;
        int length = std::min<size_t>(MAX_CLOSE_PAYLOAD, message.length());

        // todo: here we start a timeout and handle it accordingly in the timeout handler

        WebSocketData *webSocketData = (WebSocketData *) static_dispatch(us_ssl_socket_ext, us_socket_ext)((SOCKET_TYPE *) this);

        /* We postpone any FIN sending to either drainage or uncorking */
        webSocketData->isShuttingDown = true;

        /* Format and send the close frame */
        char closePayload[MAX_CLOSE_PAYLOAD + 2];
        int closePayloadLength = protocol::formatClosePayload(closePayload, code, message.data(), length);

        // but what if we are NOT corked, THEN we can FIN here if we succeeded

        // if we are corked and send returns true we cannot know for sure if we can fin
        send(std::string_view(closePayload, closePayloadLength), OpCode::CLOSE);

        // why should we fin here?
        //us_socket_shutdown((us_socket *) this);

        /* Emit close event */
        WebSocketContextData<SSL> *webSocketContextData = (WebSocketContextData<SSL> *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)(
            (SOCKET_CONTEXT_TYPE *) static_dispatch(us_ssl_socket_get_context, us_socket_get_context)((SOCKET_TYPE *) this)
        );
        if (webSocketContextData->closeHandler) {
            webSocketContextData->closeHandler(this, code, message);
        }
    }
};

}

#endif // WEBSOCKET_H
