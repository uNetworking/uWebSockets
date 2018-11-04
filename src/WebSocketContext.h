#ifndef WEBSOCKETCONTEXT_H
#define WEBSOCKETCONTEXT_H

#include "StaticDispatch.h"
#include "WebSocketContextData.h"

// the context depend on the PARSER but not the formatter!
#include "WebSocketProtocol.h"

#include "WebSocketData.h"

#include "AsyncSocket.h"

namespace uWS {

template <bool SSL>
struct WebSocketContext : StaticDispatch<SSL> {
    template <bool> friend struct TemplatedApp;
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

    // I don't even.. merge this with the context itself!
    template <bool isServer>
    struct WebSocketProtcolImplementation {
        static bool setCompressed(uWS::WebSocketState<isServer> *wState) {
            std::cout << "set compressed" << std::endl;
            return true;
        }

        static void forceClose(uWS::WebSocketState<isServer> *wState) {
            std::cout << "force close" << std::endl;
        }

        static bool handleFragment(char *data, size_t length, unsigned int remainingBytes, int opCode, bool fin, uWS::WebSocketState<isServer> *webSocketState, void *s) {

            // this is maybe not the most elegant but who cares
            WebSocketContextData<SSL> *webSocketContextData = (WebSocketContextData<SSL> *) us_socket_context_ext(us_socket_get_context((us_socket *) s));

            // emit message
            webSocketContextData->messageHandler((WebSocket<SSL, true> *) s, std::string_view(data, length), (uWS::OpCode) opCode);

            // the only thing here to check is probably closed

            // why does it not do anything immediately on true?
            return false;
        }

        static bool refusePayloadLength(uint64_t length, uWS::WebSocketState<isServer> *wState) {
            //std::cout << "refusepayloadlength" << std::endl;
            return false;
        }
    };

    WebSocketContext<SSL> *init() {

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
            uWS::WebSocketProtocol<true, WebSocketProtcolImplementation<true>>::consume(data, length, wsState, s);


            webSocket->uncork();

            return s;
        });

        /* Handle HTTP write out (note: SSL_read may trigger this spuriously, the app need to handle spurious calls) */
        static_dispatch(us_ssl_socket_context_on_writable, us_socket_context_on_writable)(getSocketContext(), [](auto *s) {

            std::cout << "websocket writable" << std::endl;

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
