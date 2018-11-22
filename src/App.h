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

#ifndef APP_H
#define APP_H

/* An app is a convenience wrapper of some of the most used fuctionalities and allows a
 * builder-pattern kind of init. Apps operate on the implicit thread local Loop */

#include "HttpContext.h"
#include "HttpResponse.h"
#include "WebSocketContext.h"
#include "WebSocket.h"

#include "libwshandshake.hpp"

namespace uWS {

template <bool SSL>
struct TemplatedApp {
private:
    /* The app always owns at least one http context, but creates websocket contexts on demand */
    HttpContext<SSL> *httpContext;
public:

    ~TemplatedApp() {

    }

    TemplatedApp(const TemplatedApp &other) {
        httpContext = other.httpContext;
    }

    TemplatedApp(us_ssl_socket_context_options sslOptions = {}) {
        httpContext = uWS::HttpContext<SSL>::create(uWS::Loop::defaultLoop(), &sslOptions);
    }

    struct WebSocketBehavior {
        std::function<void(uWS::WebSocket<SSL, true> *, HttpRequest *)> open = nullptr;
        std::function<void(uWS::WebSocket<SSL, true> *, std::string_view, uWS::OpCode)> message = nullptr;
    };

    template <class UserData>
    TemplatedApp &ws(std::string pattern, WebSocketBehavior &&behavior) {
        /* Every route has its own websocket context with its own behavior and user data type */
        auto *webSocketContext = WebSocketContext<SSL, true>::create(Loop::defaultLoop(), (typename StaticDispatch<SSL>::SOCKET_CONTEXT_TYPE *) httpContext);

        /* Copy all handlers */
        webSocketContext->getExt()->messageHandler = behavior.message;

        return get(pattern, [webSocketContext, this, behavior](auto *res, auto *req) {
            /* If we have this header set, it's a websocket */
            std::string_view secWebSocketKey = req->getHeader("sec-websocket-key");
            if (secWebSocketKey.length()) {

                // todo: negotiate extensions such as compression here and pass autobahn fully

                // note: OpenSSL can be used here to speed this up somewhat
                char secWebSocketAccept[29] = {};
                WebSocketHandshake::generate(secWebSocketKey.data(), secWebSocketAccept);

                res->writeStatus("101 Switching Protocols")
                    ->writeHeader("Upgrade", "websocket")
                    ->writeHeader("Connection", "Upgrade")
                    ->writeHeader("Sec-WebSocket-Accept", secWebSocketAccept)
                    ->end();

                /* Adopting a socket invalidates it, do not rely on it directly to carry any data */
                WebSocket<SSL, true> *webSocket = (WebSocket<SSL, true> *) StaticDispatch<SSL>::static_dispatch(us_ssl_socket_context_adopt_socket, us_socket_context_adopt_socket)(
                            (typename StaticDispatch<SSL>::SOCKET_CONTEXT_TYPE *) webSocketContext, (typename StaticDispatch<SSL>::SOCKET_TYPE *) res, /*sizeof(WebSocketData)*/ 150);

                httpContext->upgradeToWebSocket(
                            webSocket->init()
                            );

                if (behavior.open) {
                    behavior.open(webSocket, req);
                }

            } else {
                /* For now we do not support having HTTP and websocket routes on the same URL */
                res->close();
            }
        });

        return *this;
    }

    TemplatedApp &get(std::string pattern, std::function<void(HttpResponse<SSL> *, HttpRequest *)> handler) {
        httpContext->onHttp("get", pattern, handler);
        return *this;
    }

    TemplatedApp &post(std::string pattern, std::function<void(HttpResponse<SSL> *, HttpRequest *)> handler) {
        httpContext->onHttp("post", pattern, handler);
        return *this;
    }

    TemplatedApp &options(std::string pattern, std::function<void(HttpResponse<SSL> *, HttpRequest *)> handler) {
        httpContext->onHttp("options", pattern, handler);
        return *this;
    }

    TemplatedApp &del(std::string pattern, std::function<void(HttpResponse<SSL> *, HttpRequest *)> handler) {
        httpContext->onHttp("delete", pattern, handler);
        return *this;
    }

    TemplatedApp &patch(std::string pattern, std::function<void(HttpResponse<SSL> *, HttpRequest *)> handler) {
        httpContext->onHttp("patch", pattern, handler);
        return *this;
    }

    TemplatedApp &put(std::string pattern, std::function<void(HttpResponse<SSL> *, HttpRequest *)> handler) {
        httpContext->onHttp("put", pattern, handler);
        return *this;
    }

    TemplatedApp &head(std::string pattern, std::function<void(HttpResponse<SSL> *, HttpRequest *)> handler) {
        httpContext->onHttp("head", pattern, handler);
        return *this;
    }

    TemplatedApp &connect(std::string pattern, std::function<void(HttpResponse<SSL> *, HttpRequest *)> handler) {
        httpContext->onHttp("connect", pattern, handler);
        return *this;
    }

    TemplatedApp &trace(std::string pattern, std::function<void(HttpResponse<SSL> *, HttpRequest *)> handler) {
        httpContext->onHttp("trace", pattern, handler);
        return *this;
    }

    TemplatedApp &unhandled(std::function<void(HttpResponse<SSL> *, HttpRequest *)> handler) {
        httpContext->onUnhandled(handler);
        return *this;
    }

    TemplatedApp &listen(int port, std::function<void(us_listen_socket *)> handler) {
        handler(httpContext->listen(nullptr, port, 0));
        return *this;
    }

    TemplatedApp &run() {
        uWS::run();
        return *this;
    }

};

typedef TemplatedApp<false> App;
typedef TemplatedApp<true> SSLApp;

}

#endif // APP_H
