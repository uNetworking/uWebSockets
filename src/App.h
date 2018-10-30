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
    HttpContext<SSL> *httpContext;

    // the app does not own a websocket context, it is created on .ws(...) calls on demand!

public:

    ~TemplatedApp() {

    }

    TemplatedApp(const TemplatedApp &other) {
        httpContext = other.httpContext;
    }

    TemplatedApp(us_ssl_socket_context_options sslOptions = {}) {
        httpContext = uWS::HttpContext<SSL>::create(uWS::Loop::defaultLoop(), &sslOptions);

        // construct the websocket cintext? no! on demand!
    }

    // this method creates a new websocket context and attaches it to a path
    TemplatedApp &ws(std::string pattern, std::function<void(void *, HttpRequest *)> connectHandler, std::function<void(uWS::WebSocket<SSL, true> *, std::string_view)> messageHandler) {
        // init the websocket context here!
        uWS::WebSocketContext<SSL> *webSocketContext = uWS::WebSocketContext<SSL>::create(uWS::Loop::defaultLoop(), (typename StaticDispatch<SSL>::SOCKET_CONTEXT_TYPE *) httpContext);

        webSocketContext->getExt()->messageHandler = messageHandler;

        return get(pattern, [webSocketContext, this, connectHandler](auto *res, auto *req) {

            std::string_view secWebSocketKey = req->getHeader("sec-websocket-key");
            if (secWebSocketKey.length()) {

                // note: OpenSSL can be used here to speed this up somewhat
                char secWebSocketAccept[29] = {};
                WebSocketHandshake::generate(secWebSocketKey.data(), secWebSocketAccept);

                res->writeStatus("101 Switching Protocols")
                    ->writeHeader("Upgrade", "websocket")
                    ->writeHeader("Connection", "Upgrade")
                    ->writeHeader("Sec-WebSocket-Accept", secWebSocketAccept)
                    ->end();

                std::cout << "Adopting" << std::endl;

                // adopting will immediately delete the socket! we cannot rely on reading anything on it
                // rely on http context data

                // todo: sizeof websocket
                WebSocket<SSL, true> *webSocket = (WebSocket<SSL, true> *) StaticDispatch<SSL>::static_dispatch(us_ssl_socket_context_adopt_socket, us_socket_context_adopt_socket)(
                            (typename StaticDispatch<SSL>::SOCKET_CONTEXT_TYPE *) webSocketContext, (typename StaticDispatch<SSL>::SOCKET_TYPE *) res, 150);

                webSocket->init();

                std::cout << "adopted" << std::endl;

                httpContext->upgradeToWebSocket(
                            webSocket
                            );

                // we should hand the new socket to the handler
                connectHandler(webSocket, req);

            } else {

                std::cout << "This is not a websocket so fuck off!" << std::endl;

                // maybe pass this one to a HTTP handler on the websocket

                // note: this calls the http close handler inline
                res->close();
            }


        });
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
