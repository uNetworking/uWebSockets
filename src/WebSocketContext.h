#ifndef WEBSOCKETCONTEXT_H
#define WEBSOCKETCONTEXT_H

#include "StaticDispatch.h"
#include "WebSocketContextData.h"

// the context depend on the PARSER but not the formatter!
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
        return (WebSocketContextData<SSL> *) us_socket_context_ext((SOCKET_CONTEXT_TYPE *) this);
    }

    // could still lie in its own struct!
    static bool setCompressed(uWS::WebSocketState<isServer> *wState) {
        std::cout << "set compressed" << std::endl;
        return false; // do not support it
    }

    // todo: pass along user!
    static void forceClose(uWS::WebSocketState<isServer> *wState, void *s) {
        std::cout << "force close" << std::endl;

        us_socket_close((us_socket *) s);

    }

    static bool handleFragment(char *data, size_t length, unsigned int remainingBytes, int opCode, bool fin, uWS::WebSocketState<isServer> *webSocketState, void *s) {

        // this is maybe not the most elegant but who cares
        WebSocketContextData<SSL> *webSocketContextData = (WebSocketContextData<SSL> *) us_socket_context_ext(us_socket_get_context((us_socket *) s));

        // we need to get the WebSocket data also!
        WebSocketData *webSocketData = (WebSocketData *) us_socket_ext((us_socket *) s);

        if (opCode < 3) {

            if (!remainingBytes && fin && !webSocketData->fragmentBuffer.length()) {

                /* Check text messages for Utf-8 validity */
                if (opCode == 1 && !WebSocketProtocol<isServer, WebSocketContext<SSL, isServer>>::isValidUtf8((unsigned char *) data, length)) {
                    forceClose(webSocketState, s);
                    return true;
                }

                webSocketContextData->messageHandler((WebSocket<SSL, true> *) s, std::string_view(data, length), (uWS::OpCode) opCode);

                // todo: check if shut down or shutting down (shut down from websocket perspective)
                // if so, then return true

            } else {
                /* Allocate fragment buffer up front first time */
                if (!webSocketData->fragmentBuffer.length()) {
                    std::cout << "Resizing buffers to " << (length + remainingBytes) << " bytes" << std::endl;
                    webSocketData->fragmentBuffer.reserve(length + remainingBytes);
                }

                webSocketData->fragmentBuffer.append(data, length);

                std::cout << "buffering incomplete fragment: " << webSocketData->fragmentBuffer.length() << " added " << length << std::endl;

                /* Are we done now? */
                // what if we don't have any remaining bytes yet we are not fin? forceclose!
                if (!remainingBytes && fin) {

                    std::cout << "GOT FINAL FRAGMENT!" << std::endl;

                    // reset length and data ptrs
                    length = webSocketData->fragmentBuffer.length();
                    data = webSocketData->fragmentBuffer.data();


                    /* Check text messages for Utf-8 validity */
                    if (opCode == 1 && !WebSocketProtocol<isServer, WebSocketContext<SSL, isServer>>::isValidUtf8((unsigned char *) data, length)) {
                        forceClose(webSocketState, s);
                        return true;
                    }

                    webSocketContextData->messageHandler((WebSocket<SSL, isServer> *) s, std::string_view(data, length), (uWS::OpCode) opCode);

                    // todo: check if shut down or shutting down (shut down from websocket perspective)
                    // if so, then return true


                    webSocketData->fragmentBuffer.clear();
                }

            }


        } else {
            WebSocket<SSL, isServer> *webSocket = (WebSocket<SSL, isServer> *) s;

            if (!remainingBytes && fin && !webSocketData->controlTipLength) {
                if (opCode == CLOSE) {
                    typename WebSocketProtocol<isServer, WebSocketContext<SSL, isServer>>::CloseFrame closeFrame = WebSocketProtocol<isServer, WebSocketContext<SSL, isServer>>::parseClosePayload(data, length);
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
                // todo, buffer control frames

                std::cout << "control frames! BUFFFERRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRR" << std::endl;


                // unsure how big this is?
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

                    // not optimal but slow path
                    webSocketData->fragmentBuffer.resize(webSocketData->fragmentBuffer.length() - webSocketData->controlTipLength);
                    webSocketData->controlTipLength = 0;
                }




            }


        }



        // the only thing here to check is probably closed

        // why does it not do anything immediately on true?
        return false;
    }

    static bool refusePayloadLength(uint64_t length, uWS::WebSocketState<isServer> *wState) {
        //std::cout << "refusepayloadlength" << std::endl;
        return false;
    }

    WebSocketContext<SSL, isServer> *init() {

        /* I guess open is never called */

        /* Handle socket disconnections */
        static_dispatch(us_ssl_socket_context_on_close, us_socket_context_on_close)(getSocketContext(), [](auto *s) {

            std::cout << "close!" << std::endl;

            return s;
        });

        /* Handle HTTP data streams */
        static_dispatch(us_ssl_socket_context_on_data, us_socket_context_on_data)(getSocketContext(), [](auto *s, char *data, int length) {


            AsyncSocket<SSL> *webSocket = (AsyncSocket<SSL> *) s;

            webSocket->cork();

            // get the data
            WebSocketData *wsState = (WebSocketData *) us_socket_ext(s);

            // this parser requires almost no time -> 215k req/sec of 215k possible
            uWS::WebSocketProtocol<isServer, WebSocketContext<SSL, isServer>>::consume(data, length, wsState, s);


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

            webSocket->write(nullptr, 0); // drainage - also check for shutdown!

            return s;
        });

        /* Handle FIN, HTTP does not support half-closed sockets, so simply close */
        static_dispatch(us_ssl_socket_context_on_end, us_socket_context_on_end)(getSocketContext(), [](auto *s) {

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
