/*
 * Authored by Alex Hultman, 2018-2019.
 * Intellectual property of third-party.

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

#include "WebSocketContextData.h"
#include "WebSocketProtocol.h"
#include "WebSocketData.h"
#include "WebSocket.h"

namespace uWS {

template <bool SSL, bool isServer>
struct WebSocketContext {
    template <bool> friend struct TemplatedApp;
    template <bool, class> friend class WebSocketProtocol;
private:
    WebSocketContext() = delete;

    us_new_socket_context_t *getSocketContext() {
        return (us_new_socket_context_t *) this;
    }

    WebSocketContextData<SSL> *getExt() {
        return (WebSocketContextData<SSL> *) us_new_socket_context_ext(SSL, (us_new_socket_context_t *) this);
    }

    /* If we have negotiated compression, set this frame compressed */
    static bool setCompressed(uWS::WebSocketState<isServer> *wState, void *s) {
        WebSocketData *webSocketData = (WebSocketData *) us_new_socket_ext(SSL, (us_new_socket_t *) s);

        if (webSocketData->compressionStatus == WebSocketData::CompressionStatus::ENABLED) {
            webSocketData->compressionStatus = WebSocketData::CompressionStatus::COMPRESSED_FRAME;
            return true;
        } else {
            return false;
        }
    }

    static void forceClose(uWS::WebSocketState<isServer> *wState, void *s) {
        us_new_socket_close(SSL, (us_new_socket_t *) s);
    }

    /* Returns true on breakage */
    static bool handleFragment(char *data, size_t length, unsigned int remainingBytes, int opCode, bool fin, uWS::WebSocketState<isServer> *webSocketState, void *s) {
        /* WebSocketData and WebSocketContextData */
        WebSocketContextData<SSL> *webSocketContextData = (WebSocketContextData<SSL> *) us_new_socket_context_ext(SSL,
            us_new_socket_context(SSL, (us_new_socket_t *) s)
        );
        WebSocketData *webSocketData = (WebSocketData *) us_new_socket_ext(SSL, (us_new_socket_t *) s);

        /* Is this a non-control frame? */
        if (opCode < 3) {
            /* Did we get everything in one go? */
            if (!remainingBytes && fin && !webSocketData->fragmentBuffer.length()) {

                /* Handle compressed frame */
                if (webSocketData->compressionStatus == WebSocketData::CompressionStatus::COMPRESSED_FRAME) {
                        webSocketData->compressionStatus = WebSocketData::CompressionStatus::ENABLED;

                        LoopData *loopData = (LoopData *)us_loop_ext(
                            us_new_socket_context_loop(SSL,
                                us_new_socket_context(SSL, (us_new_socket_t *)s)
                                )
                        );

                        std::string_view inflatedFrame = loopData->inflationStream->inflate(loopData->zlibContext, {data, length}, webSocketContextData->maxPayloadLength);
                        if (!inflatedFrame.length()) {
                            forceClose(webSocketState, s);
                            return true;
                        } else {
                            data = (char *) inflatedFrame.data();
                            length = inflatedFrame.length();
                        }
                }

                /* Check text messages for Utf-8 validity */
                if (opCode == 1 && !protocol::isValidUtf8((unsigned char *) data, length)) {
                    forceClose(webSocketState, s);
                    return true;
                }

                /* Emit message event & break if we are closed or shut down when returning */
                if (webSocketContextData->messageHandler) {
                    webSocketContextData->messageHandler((WebSocket<SSL, isServer> *) s, std::string_view(data, length), (uWS::OpCode) opCode);
                    if (us_new_socket_is_closed(SSL, (us_new_socket_t *)s) || webSocketData->isShuttingDown) {
                        return true;
                    }
                }
            } else {
                /* Allocate fragment buffer up front first time */
                if (!webSocketData->fragmentBuffer.length()) {
                    webSocketData->fragmentBuffer.reserve(length + remainingBytes);
                }
                webSocketData->fragmentBuffer.append(data, length);

                /* Are we done now? */
                // todo: what if we don't have any remaining bytes yet we are not fin? forceclose!
                if (!remainingBytes && fin) {

                    /* Handle compression */
                    if (webSocketData->compressionStatus == WebSocketData::CompressionStatus::COMPRESSED_FRAME) {
                            webSocketData->compressionStatus = WebSocketData::CompressionStatus::ENABLED;

                            // what's really the story here?
                            webSocketData->fragmentBuffer.append("....");

                            LoopData *loopData = (LoopData *) us_loop_ext(
                                us_new_socket_context_loop(SSL,
                                    us_new_socket_context(SSL, (us_new_socket_t *) s)
                                )
                            );

                            std::string_view inflatedFrame = loopData->inflationStream->inflate(loopData->zlibContext, {webSocketData->fragmentBuffer.data(), webSocketData->fragmentBuffer.length() - 4}, webSocketContextData->maxPayloadLength);
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
                    if (opCode == 1 && !protocol::isValidUtf8((unsigned char *) data, length)) {
                        forceClose(webSocketState, s);
                        return true;
                    }

                    /* Emit message and check for shutdown or close */
                    if (webSocketContextData->messageHandler) {
                        webSocketContextData->messageHandler((WebSocket<SSL, isServer> *) s, std::string_view(data, length), (uWS::OpCode) opCode);
                        if (us_new_socket_is_closed(SSL, (us_new_socket_t *)s) || webSocketData->isShuttingDown) {
                            return true;
                        }
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
                    auto closeFrame = protocol::parseClosePayload(data, length);
                    webSocket->end(closeFrame.code, std::string_view(closeFrame.message, closeFrame.length));
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
                        protocol::CloseFrame closeFrame = protocol::parseClosePayload(controlBuffer, webSocketData->controlTipLength);
                        webSocket->end(closeFrame.code, std::string_view(closeFrame.message, closeFrame.length));
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

    static bool refusePayloadLength(uint64_t length, uWS::WebSocketState<isServer> *wState, void *s) {
        WebSocketContextData<SSL> *webSocketContextData = (WebSocketContextData<SSL> *) us_new_socket_context_ext(SSL,
            us_new_socket_context(SSL, (us_new_socket_t *)s)
            );

        /* Return true for refuse, false for accept */
        return webSocketContextData->maxPayloadLength < length;
    }

    WebSocketContext<SSL, isServer> *init() {
        /* Adopting a socket does not trigger open event.
         * We arreive as WebSocket with timeout set and
         * any backpressure from HTTP state kept. */

        /* Handle socket disconnections */
        us_new_socket_context_on_close(SSL, getSocketContext(), [](auto *s) {

            /* For whatever reason, if we already have emitted close event, do not emit it again */
            WebSocketData *webSocketData = (WebSocketData *) (us_new_socket_ext(SSL, s));
            if (!webSocketData->isShuttingDown) {
                /* Emit close event */
                WebSocketContextData<SSL> *webSocketContextData = (WebSocketContextData<SSL> *) us_new_socket_context_ext(SSL,
                    us_new_socket_context(SSL, (us_new_socket_t *)s)
                    );

                if (webSocketContextData->closeHandler) {
                    webSocketContextData->closeHandler((WebSocket<SSL, true> *) s, 1006, {});
                }
            }

            /* Destruct in-placed data struct */
            webSocketData->~WebSocketData();

            return s;
        });

        /* Handle WebSocket data streams */
        us_new_socket_context_on_data(SSL, getSocketContext(), [](auto *s, char *data, int length) {

            /* We need the websocket data */
            WebSocketData *webSocketData = (WebSocketData *) (us_new_socket_ext(SSL, s));

            /* When in websocket shutdown mode, we do not care for ANY message, whether responding close frame or not.
             * We only care for the TCP FIN really, not emitting any message after closing is key */
            if (webSocketData->isShuttingDown) {
                return s;
            }

            /* Everytime we get data, we reset the timeout to our idleTimeout, that's the only timer we have */

            /* If not in websocket shutdown state, for every */

            // återställ inte om vi är i shutdown state, dvs, ge den inte massa tid på sig att skicka massa skit-frames mellan och upphålla oss!
            WebSocketContextData<SSL> *webSocketContextData = (WebSocketContextData<SSL> *) us_new_socket_context_ext(SSL,
                us_new_socket_context(SSL, (us_new_socket_t *) s)
                );

            us_new_socket_timeout(SSL, (us_new_socket_t *) s, webSocketContextData->idleTimeout);


            /* We always cork on data */
            AsyncSocket<SSL> *webSocket = (AsyncSocket<SSL> *) s;
            webSocket->cork();

            /* This parser has virtually no overhead */
            uWS::WebSocketProtocol<isServer, WebSocketContext<SSL, isServer>>::consume(data, length, (WebSocketState<isServer> *) webSocketData, s);

            // todo: we need to check for close and shutdown here? as we just emitted a bunch of message/close events!

            // todo: check for failures here just like for HTTP
            webSocket->uncork();

            // cannot do anything else if closed
            if (us_new_socket_is_closed(SSL, (us_new_socket_t *) s)) {
                return s;
            }

            // I guess we need to check drain here - emit drain if we had to poll for writable

            // are we shutdown? can onnly call this if we did succeed uncork!
            if (webSocketData->isShuttingDown) {
                webSocket->shutdown();
            }

            return s;
        });

        /* Handle HTTP write out (note: SSL_read may trigger this spuriously, the app need to handle spurious calls) */
        us_new_socket_context_on_writable(SSL, getSocketContext(), [](auto *s) {

            /* It makes sense to check for us_is_shut_down here and return if so, to avoid shutting down twice */
            if (us_new_socket_is_shut_down(SSL, (us_new_socket_t *) s)) {
                return s;
            }

            AsyncSocket<SSL> *webSocket = (AsyncSocket<SSL> *) s;
            WebSocketData *webSocketData = (WebSocketData *)(us_new_socket_ext(SSL, s));

            /* We store old backpressure since it is unclear whether write drained anything */
            int backpressure = webSocket->getBufferedAmount();

            /* Drain as much as possible */
            webSocket->write(nullptr, 0);

            /* Behavior: if we actively drain backpressure, always reset timeout (even if we are in shutdown) */
            if (backpressure < webSocket->getBufferedAmount()) {
                WebSocketContextData<SSL> *webSocketContextData = (WebSocketContextData<SSL> *) us_new_socket_context_ext(SSL,
                    us_new_socket_context(SSL, (us_new_socket_t *)s)
                    );
                webSocket->timeout(webSocketContextData->idleTimeout);
            }

            /* Are we in (WebSocket) shutdown mode? */
            if (webSocketData->isShuttingDown) {
                /* Check if we just now drained completely */
                if (webSocket->getBufferedAmount() == 0) {
                    /* Now perform the actual TCP/TLS shutdown which was postponed due to backpressure */
                    webSocket->shutdown();
                }
            } else if (backpressure > webSocket->getBufferedAmount()) {
                /* Only call drain if we actually drained backpressure */
                WebSocketContextData<SSL> *webSocketContextData = (WebSocketContextData<SSL> *) us_new_socket_context_ext(SSL,
                    us_new_socket_context(SSL, (us_new_socket_t *)s)
                    );
                if (webSocketContextData->drainHandler) {
                    webSocketContextData->drainHandler((WebSocket<SSL, isServer> *) s);
                }
                /* No need to check for closed here as we leave the handler immediately*/
            }

            return s;
        });

        /* Handle FIN, HTTP does not support half-closed sockets, so simply close */
        us_new_socket_context_on_end(SSL, getSocketContext(), [](auto *s) {

            /* If we get a fin, we just close I guess */
            us_new_socket_close(SSL, (us_new_socket_t *) s);

            return s;
        });

        /* Handle socket timeouts, simply close them so to not confuse client with FIN */
        us_new_socket_context_on_timeout(SSL, getSocketContext(), [](auto *s) {

            /* Timeout is very simple; we just close it */
            us_new_socket_close(SSL, (us_new_socket_t *) s);

            return s;
        });

        return this;
    }

    void free() {
        WebSocketContextData<SSL> *webSocketContextData = (WebSocketContextData<SSL> *) us_new_socket_context_ext(SSL, (us_new_socket_context_t *) this);
        webSocketContextData->~WebSocketContextData();

        us_new_socket_context_free(SSL, (us_new_socket_context_t *) this);
    }

public:
    /* WebSocket contexts are always child contexts to a HTTP context so no SSL options are needed as they are inherited */
    static WebSocketContext *create(Loop *loop, us_new_socket_context_t *parentSocketContext) {
        WebSocketContext *webSocketContext = (WebSocketContext *) us_new_create_child_socket_context(SSL, parentSocketContext, sizeof(WebSocketContextData<SSL>));
        if (!webSocketContext) {
            return nullptr;
        }

        /* Init socket context data */
        new ((WebSocketContextData<SSL> *) us_new_socket_context_ext(SSL, (us_new_socket_context_t *)webSocketContext)) WebSocketContextData<SSL>;
        return webSocketContext->init();
    }
};

}

#endif // WEBSOCKETCONTEXT_H
