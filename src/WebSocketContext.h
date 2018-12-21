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

#ifndef WEBSOCKETCONTEXT_H
#define WEBSOCKETCONTEXT_H

#include "StaticDispatch.h"
#include "WebSocketContextData.h"
#include "WebSocketProtocol.h"
#include "WebSocketData.h"
#include "AsyncSocket.h"

namespace uWS {

template <bool SSL, bool isServer>
struct WebSocketContext : StaticDispatch<SSL> {
    template <bool> friend struct TemplatedApp;
    template <bool, class> friend struct WebSocketProtocol;
private:
    using SOCKET_CONTEXT_TYPE = typename StaticDispatch<SSL>::SOCKET_CONTEXT_TYPE;
    using SOCKET_TYPE = typename StaticDispatch<SSL>::SOCKET_TYPE;
    using StaticDispatch<SSL>::static_dispatch;
    WebSocketContext() = delete;

    SOCKET_CONTEXT_TYPE *getSocketContext() {
        return (SOCKET_CONTEXT_TYPE *) this;
    }

    WebSocketContextData<SSL> *getExt() {
        return (WebSocketContextData<SSL> *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)((SOCKET_CONTEXT_TYPE *) this);
    }

    /* If we have negotiated compression, set this frame compressed */
    static bool setCompressed(uWS::WebSocketState<isServer> *wState, void *s) {
        //WebSocketData *webSocketData = (WebSocketData *) us_socket_ext((us_socket *) s);

        std::cout << "set compressed" << std::endl;

        WebSocketData *webSocketData = (WebSocketData *) static_dispatch(us_ssl_socket_ext, us_socket_ext)((SOCKET_TYPE *) s);

        if (webSocketData->compressionStatus == WebSocketData::CompressionStatus::ENABLED) {
            webSocketData->compressionStatus = WebSocketData::CompressionStatus::COMPRESSED_FRAME;
            return true;
        } else {
            return false;
        }
    }

    static void forceClose(uWS::WebSocketState<isServer> *wState, void *s) {
        us_socket_close((us_socket *) s);
    }

    /* Returns true on breakage */
    static bool handleFragment(char *data, size_t length, unsigned int remainingBytes, int opCode, bool fin, uWS::WebSocketState<isServer> *webSocketState, void *s) {
        /* WebSocketData and WebSocketContextData */
        //WebSocketContextData<SSL> *webSocketContextData = (WebSocketContextData<SSL> *) us_socket_context_ext(us_socket_get_context((us_socket *) s));
        //WebSocketData *webSocketData = (WebSocketData *) us_socket_ext((us_socket *) s);

        WebSocketContextData<SSL> *webSocketContextData = (WebSocketContextData<SSL> *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)(

        static_dispatch(us_ssl_socket_get_context, us_socket_get_context)((SOCKET_TYPE *) s)

                    );

        WebSocketData *webSocketData = (WebSocketData *) static_dispatch(us_ssl_socket_ext, us_socket_ext)((SOCKET_TYPE *) s);

        //std::cout << "ho" << std::endl;

        /* Is this a non-control frame? */
        if (opCode < 3) {
            /* Did we get everything in one go? */
            if (!remainingBytes && fin && !webSocketData->fragmentBuffer.length()) {

                /* Handle compressed frame */
                if (webSocketData->compressionStatus == WebSocketData::CompressionStatus::COMPRESSED_FRAME) {
                        webSocketData->compressionStatus = WebSocketData::CompressionStatus::ENABLED;

                        //LoopData *loopData = (LoopData *) us_loop_ext(us_socket_context_loop(us_socket_get_context((us_socket *) s)));

                        LoopData *loopData = (LoopData *) us_loop_ext(


                                    static_dispatch(us_ssl_socket_context_loop, us_socket_context_loop)(

                                    static_dispatch(us_ssl_socket_get_context, us_socket_get_context)((SOCKET_TYPE *) s)

                                        )

                                    );

                        std::string_view inflatedFrame = loopData->inflationStream->inflate({data, length});
                        if (!inflatedFrame.length()) {
                            forceClose(webSocketState, s);
                            return true;
                        } else {
                            data = (char *) inflatedFrame.data();
                            length = inflatedFrame.length();
                        }
                }

                /* Check text messages for Utf-8 validity */
                if (opCode == 1 && !WebSocketProtocol<isServer, WebSocketContext<SSL, isServer>>::isValidUtf8((unsigned char *) data, length)) {
                    forceClose(webSocketState, s);
                    return true;
                }

                /* Emit message event & break if we are closed or shut down when returning */
                webSocketContextData->messageHandler((WebSocket<SSL, isServer> *) s, std::string_view(data, length), (uWS::OpCode) opCode);
                if (us_socket_is_closed((us_socket *) s) || webSocketData->isShuttingDown) {
                    return true;
                }
            } else {
                /* Allocate fragment buffer up front first time */
                if (!webSocketData->fragmentBuffer.length()) {
                    webSocketData->fragmentBuffer.reserve(length + remainingBytes);
                }
                webSocketData->fragmentBuffer.append(data, length);

                /* Are we done now? */
                // what if we don't have any remaining bytes yet we are not fin? forceclose!
                if (!remainingBytes && fin) {

                    /* Handle compression */
                    if (webSocketData->compressionStatus == WebSocketData::CompressionStatus::COMPRESSED_FRAME) {
                            webSocketData->compressionStatus = WebSocketData::CompressionStatus::ENABLED;

                            // what's really the story here?
                            webSocketData->fragmentBuffer.append("....");

                            //LoopData *loopData = (LoopData *) us_loop_ext(us_socket_context_loop(us_socket_get_context((us_socket *) s)));

                            //LoopData *loopData = (LoopData *) us_loop_ext(us_socket_context_loop(us_socket_get_context((us_socket *) s)));

                            LoopData *loopData = (LoopData *) us_loop_ext(


                                        static_dispatch(us_ssl_socket_context_loop, us_socket_context_loop)(

                                        static_dispatch(us_ssl_socket_get_context, us_socket_get_context)((SOCKET_TYPE *) s)

                                            )

                                        );


                            std::string_view inflatedFrame = loopData->inflationStream->inflate({webSocketData->fragmentBuffer.data(), webSocketData->fragmentBuffer.length() - 4});
                            if (!inflatedFrame.length()) {
                                forceClose(webSocketState, s);
                                return true;
                            } else {
                                data = (char *) inflatedFrame.data();
                                length = inflatedFrame.length();
                            }


                    } else {
                        // reset length and data ptrs
                        length = webSocketData->fragmentBuffer.length();
                        data = webSocketData->fragmentBuffer.data();
                    }

                    /* Check text messages for Utf-8 validity */
                    if (opCode == 1 && !WebSocketProtocol<isServer, WebSocketContext<SSL, isServer>>::isValidUtf8((unsigned char *) data, length)) {
                        forceClose(webSocketState, s);
                        return true;
                    }

                    /* Emit message and check for shutdown or close */
                    webSocketContextData->messageHandler((WebSocket<SSL, isServer> *) s, std::string_view(data, length), (uWS::OpCode) opCode);
                    if (us_socket_is_closed((us_socket *) s) || webSocketData->isShuttingDown) {
                        return true;
                    }

                    /* If we shutdown or closed, this will be taken care of elsewhere */
                    webSocketData->fragmentBuffer.clear();
                }
            }
        } else {
            /* Control frames need the websocket to send pings, pongs and close */
            WebSocket<SSL, isServer> *webSocket = (WebSocket<SSL, isServer> *) s;

            if (!remainingBytes && fin && !webSocketData->controlTipLength) {
                if (opCode == CLOSE) {
                    auto closeFrame = WebSocketProtocol<isServer, WebSocketContext<SSL, isServer>>::parseClosePayload(data, length);
                    webSocket->close(closeFrame.code, std::string_view(closeFrame.message, closeFrame.length));
                    return true;
                } else {
                    if (opCode == PING) {
                        webSocket->send(std::string_view(data, length), (OpCode) OpCode::PONG);
                        /*group->pingHandler(webSocket, data, length);
                        if (webSocket->isClosed() || webSocket->isShuttingDown()) {
                            return true;
                        }*/
                    } else if (opCode == PONG) {
                        /*group->pongHandler(webSocket, data, length);
                        if (webSocket->isClosed() || webSocket->isShuttingDown()) {
                            return true;
                        }*/
                    }
                }
            } else {
                /* Here we never mind any size optimizations as we are in the worst possible path */
                webSocketData->fragmentBuffer.append(data, length);
                webSocketData->controlTipLength += length;

                if (!remainingBytes && fin) {
                    char *controlBuffer = (char *) webSocketData->fragmentBuffer.data() + webSocketData->fragmentBuffer.length() - webSocketData->controlTipLength;
                    if (opCode == CLOSE) {
                        typename WebSocketProtocol<isServer, WebSocketContext<SSL, isServer>>::CloseFrame closeFrame = WebSocketProtocol<isServer, WebSocketContext<SSL, isServer>>::parseClosePayload(controlBuffer, webSocketData->controlTipLength);
                        webSocket->close(closeFrame.code, std::string_view(closeFrame.message, closeFrame.length));
                        return true;
                    } else {
                        if (opCode == PING) {
                            webSocket->send(std::string_view(controlBuffer, webSocketData->controlTipLength), (OpCode) OpCode::PONG);
                            /*group->pingHandler(webSocket, controlBuffer, webSocket->controlTipLength);
                            if (webSocket->isClosed() || webSocket->isShuttingDown()) {
                                return true;
                            }*/
                        } else if (opCode == PONG) {
                            /*group->pongHandler(webSocket, controlBuffer, webSocket->controlTipLength);
                            if (webSocket->isClosed() || webSocket->isShuttingDown()) {
                                return true;
                            }*/
                        }
                    }

                    /* Same here, we do not care for any particular smart allocation scheme */
                    webSocketData->fragmentBuffer.resize(webSocketData->fragmentBuffer.length() - webSocketData->controlTipLength);
                    webSocketData->controlTipLength = 0;
                }
            }
        }
        return false;
    }

    // bug: todo
    static bool refusePayloadLength(uint64_t length, uWS::WebSocketState<isServer> *wState) {

        //std::cout << "refuse payload length" << std::endl;

        /* We check if we want to accept such a frame based on size */
        // for now, accept anything
        return false;
    }

    WebSocketContext<SSL, isServer> *init() {
        /* Open is never called, we only adopt sockets */

        /* Handle socket disconnections */
        static_dispatch(us_ssl_socket_context_on_close, us_socket_context_on_close)(getSocketContext(), [](auto *s) {

            std::cout << "close!" << std::endl;

            return s;
        });

        /* Handle WebSocket data streams */
        static_dispatch(us_ssl_socket_context_on_data, us_socket_context_on_data)(getSocketContext(), [](auto *s, char *data, int length) {

            //std::cout << "websocket data" << std::endl;

            /* We always cork on data */
            AsyncSocket<SSL> *webSocket = (AsyncSocket<SSL> *) s;
            webSocket->cork();

            /* We need the websocket data */
            WebSocketData *wsState = (WebSocketData *) (static_dispatch(us_ssl_socket_ext, us_socket_ext)(s));

            // this parser requires almost no time -> 215k req/sec of 215k possible
            uWS::WebSocketProtocol<isServer, WebSocketContext<SSL, isServer>>::consume(data, length, wsState, s);


            // todo: check for failures here just like for HTTP
            webSocket->uncork();

            // are we shutdown?
            if (wsState->isShuttingDown) {
                webSocket->shutdown();
            }

            return s;
        });

        /* Handle HTTP write out (note: SSL_read may trigger this spuriously, the app need to handle spurious calls) */
        static_dispatch(us_ssl_socket_context_on_writable, us_socket_context_on_writable)(getSocketContext(), [](auto *s) {

            std::cout << "websocket writable" << std::endl;

            // we need to drain here!

            AsyncSocket<SSL> *webSocket = (AsyncSocket<SSL> *) s;

            // check for failures and shutdown just like in data event
            webSocket->write(nullptr, 0); // drainage - also check for shutdown!

            return s;
        });

        /* Handle FIN, HTTP does not support half-closed sockets, so simply close */
        static_dispatch(us_ssl_socket_context_on_end, us_socket_context_on_end)(getSocketContext(), [](auto *s) {

            // just like http, websocket does not support half-open sockets so just close here

            std::cout << "websopcket fin" << std::endl;

            /* We do not care for half closed sockets */
            //AsyncSocket<SSL> *asyncSocket = (AsyncSocket<SSL> *) s;
            //return asyncSocket->close();

            return s;
        });

        /* Handle socket timeouts, simply close them so to not confuse client with FIN */
        static_dispatch(us_ssl_socket_context_on_timeout, us_socket_context_on_timeout)(getSocketContext(), [](auto *s) {

            std::cout << "websocket timeout" << std::endl;

            /* Force close rather than gracefully shutdown and risk confusing the client with a complete download */
            //AsyncSocket<SSL> *asyncSocket = (AsyncSocket<SSL> *) s;
            //return asyncSocket->close();


            return s;
        });

        return this;
    }

public:

    // we do not need SSL options as we come from adoptions
    static WebSocketContext *create(Loop *loop, SOCKET_CONTEXT_TYPE *parentSocketContext) {
        WebSocketContext *webSocketContext;

        // todo: sizeof
        webSocketContext = (WebSocketContext *) static_dispatch(us_create_child_ssl_socket_context, us_create_child_socket_context)(parentSocketContext, 100);
        if (!webSocketContext) {
            return nullptr;
        }

        /* Init socket context data */
        new ((WebSocketContextData<SSL> *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)((SOCKET_CONTEXT_TYPE *) webSocketContext)) WebSocketContextData<SSL>();
        return webSocketContext->init();
    }

};

}

#endif // WEBSOCKETCONTEXT_H
