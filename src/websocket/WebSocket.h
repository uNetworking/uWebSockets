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

#include "libusockets.h"
#include "Socket.h"
#include "WebSocketProtocol.h"

// client or server?
template <bool SSL, bool isServer>
struct WebSocket : public Socket<SSL> {

    // this needs to hold
    struct Data : uWS::WebSocketState<isServer> {

    };

    static bool setCompressed(uWS::WebSocketState<isServer> *wState) {
        return true;
    }

    static void forceClose(uWS::WebSocketState<isServer> *wState) {

    }

    static bool handleFragment(char *data, size_t length, unsigned int remainingBytes, int opCode, bool fin, uWS::WebSocketState<isServer> *webSocketState) {

        std::cout << std::string_view(data, length) << std::endl;

                //Data *webSocketData = (Data *) static_dispatch(us_ssl_socket_ext, us_socket_ext)((SOCKET_TYPE *) this);


        //Socket<SSL>::getSocketContextExt();



    }

    static bool refusePayloadLength(uint64_t length, uWS::WebSocketState<isServer> *wState) {
        return false;
    }

    //why is this here? events are handled and emitted from the app, the app depends on websocket, not two way deps!
    void onData(char *data, int length) {

        Data *webSocketData = (Data *) Socket<SSL>::static_dispatch(us_ssl_socket_ext, us_socket_ext)((typename Socket<SSL>::SOCKET_TYPE *) this);

        uWS::WebSocketProtocol<isServer, WebSocket<SSL, isServer>>::consume(data, length, webSocketData);
    }

    WebSocket() = delete;
};

#endif // WEBSOCKET_H
