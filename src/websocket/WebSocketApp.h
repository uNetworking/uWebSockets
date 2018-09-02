#ifndef WEBSOCKETAPP_H
#define WEBSOCKETAPP_H

#include "http/HttpApp.h"
#include <vector>

// basically you have one of this for server, one for client!?
template <bool SSL>
struct WebSocketApp : HttpApp<SSL, WebSocketApp<SSL>> {

    // usings
    using HttpApp<SSL, WebSocketApp<SSL>>::static_dispatch;
    typedef typename HttpApp<SSL, WebSocketApp<SSL>>::SOCKET_CONTEXT_TYPE SOCKET_CONTEXT_TYPE;
    typedef typename HttpApp<SSL, WebSocketApp<SSL>>::SOCKET_TYPE SOCKET_TYPE;

    // constructor
    WebSocketApp(SOCKET_CONTEXT_TYPE *httpServerContext) : HttpApp<SSL, WebSocketApp<SSL>>(httpServerContext) {

    }

    // per-context "WebSocketApp" data
    template <bool isServer>
    struct WebSocketServerContextData {

        WebSocketServerContextData() {

        }

        std::function<void(WebSocket<SSL, isServer> *, std::string_view)> onMessage;
    };

    // all server contexts created with below functions
    std::vector<SOCKET_CONTEXT_TYPE *> webSocketServerContexts;
    bool lastContextIsServer;

    // register a new (server) protocol
    template <class UserData>
    WebSocketApp &onWebSocket(std::string pattern, std::function<void(HttpSocket<SSL> *, HttpRequest *, std::vector<std::string_view> *)> handler) {

        // we are going to push a server context
        lastContextIsServer = true;

        // create a new websocket child context
        SOCKET_CONTEXT_TYPE *webSocketServerContext = static_dispatch(us_create_child_ssl_socket_context, us_create_child_socket_context)(HttpApp<SSL, WebSocketApp<SSL>>::httpServerContext, sizeof(WebSocketServerContextData<true>));
        new ((WebSocketServerContextData<true> *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)(webSocketServerContext)) WebSocketServerContextData<true>();
        WebSocketApp<SSL>::webSocketServerContexts.push_back(webSocketServerContext);

        // add the behavior of it
        static_dispatch(us_ssl_socket_context_on_data, us_socket_context_on_data)(webSocketServerContext, [](auto *s, char *data, int length) {
            //WebSocketServerContextData<true> *webSocketServerContextData = (WebSocketServerContextData<true> *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)(static_dispatch(us_ssl_socket_get_context, us_socket_get_context)(s));

            ((WebSocket<SSL, true> *) s)->onData(data, length/*, webSocketServerContextData->onMessage*/);

            return s;
        });

        // todo: GET should probably be get since the parser only leaves lower case
        HttpApp<SSL, WebSocketApp<SSL>>::data->r.add("GET", pattern.c_str(), [webSocketServerContext, handler](typename HttpApp<SSL, WebSocketApp<SSL>>::Data::UserData *user, auto *args) {

            std::string_view secWebSocketKey = user->httpRequest->getHeader("sec-websocket-key");
            if (secWebSocketKey.length()) {

                // note: OpenSSL can be used here to speed this up somewhat
                char secWebSocketAccept[29] = {};
                WebSocketHandshake::generate(secWebSocketKey.data(), secWebSocketAccept);

                user->httpSocket->writeStatus("101 Switching Protocols")
                    ->writeHeader("Upgrade", "websocket")
                    ->writeHeader("Connection", "Upgrade")
                    ->writeHeader("Sec-WebSocket-Accept", secWebSocketAccept)
                    ->end("");

                // todo: transform the socket into a websocket and hand it over
                static_dispatch(us_ssl_socket_context_adopt_socket, us_socket_context_adopt_socket)(webSocketServerContext, (SOCKET_TYPE *) user->httpSocket, sizeof(typename WebSocket<SSL, true>::Data) + sizeof(UserData));

                // init the websocket data

                handler(user->httpSocket, user->httpRequest, args);
            } else {

                // maybe pass this one to a HTTP handler on the websocket

                // note: this calls the http close handler inline
                user->httpSocket->close();
            }
        });

        return *this;
    }

    // this function does in fact determine whether we are client or not based on the websocket type passed!
    template <bool isServer>
    WebSocketApp &onMessage(std::function<void(WebSocket<SSL, isServer> *, std::string_view)> handler) {

        // pop last context on the stack
        SOCKET_CONTEXT_TYPE *context = lastContextIsServer ? webSocketServerContexts.back() : nullptr;

        // get its data
        if (lastContextIsServer) {
            WebSocketServerContextData<true> *data = (WebSocketServerContextData<true> *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)(context);

            data->onMessage = handler;
        }

        return *this;
    }

    WebSocketApp &onClose(std::function<void()>) {

        return *this;
    }
};

#endif // WEBSOCKETAPP_H
