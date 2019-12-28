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

#ifndef UWS_APP_H
#define UWS_APP_H

/* An app is a convenience wrapper of some of the most used fuctionalities and allows a
 * builder-pattern kind of init. Apps operate on the implicit thread local Loop */

#include "HttpContext.h"
#include "HttpResponse.h"
#include "WebSocketContext.h"
#include "WebSocket.h"
#include "WebSocketExtensions.h"
#include "WebSocketHandshake.h"

namespace uWS {

/* Compress options (really more like PerMessageDeflateOptions) */
enum CompressOptions {
    /* Compression disabled */
    DISABLED = 0,
    /* We compress using a shared non-sliding window. No added memory usage, worse compression. */
    SHARED_COMPRESSOR = 1,
    /* We compress using a dedicated sliding window. Major memory usage added, better compression of similarly repeated messages. */
    DEDICATED_COMPRESSOR = 2
};

template <bool SSL>
struct TemplatedApp {
private:
    /* The app always owns at least one http context, but creates websocket contexts on demand */
    HttpContext<SSL> *httpContext;
    std::vector<WebSocketContext<SSL, true> *> webSocketContexts;

public:

    /* Attaches a "filter" function to track socket connections/disconnections */
    void filter(fu2::unique_function<void(HttpResponse<SSL> *, int)> &&filterHandler) {
        httpContext->filter(std::move(filterHandler));
    }

    /* Publishes a message to all websocket contexts */
    void publish(std::string_view topic, std::string_view message, OpCode opCode, bool compress = false) {
        for (auto *webSocketContext : webSocketContexts) {
            webSocketContext->getExt()->publish(topic, message, opCode, compress);
        }
    }

    ~TemplatedApp() {
        /* Let's just put everything here */
        if (httpContext) {
            httpContext->free();

            for (auto *webSocketContext : webSocketContexts) {
                webSocketContext->free();
            }
        }
    }

    /* Disallow copying, only move */
    TemplatedApp(const TemplatedApp &other) = delete;

    TemplatedApp(TemplatedApp &&other) {
        /* Move HttpContext */
        httpContext = other.httpContext;
        other.httpContext = nullptr;

        /* Move webSocketContexts */
        webSocketContexts = std::move(other.webSocketContexts);
    }

    TemplatedApp(us_socket_context_options_t options = {}) {
        httpContext = uWS::HttpContext<SSL>::create(uWS::Loop::get(), options);
    }

    bool constructorFailed() {
        return !httpContext;
    }

    struct WebSocketBehavior {
        CompressOptions compression = DISABLED;
        int maxPayloadLength = 16 * 1024;
        int idleTimeout = 120;
        int maxBackpressure = 1 * 1024 * 1204;
        fu2::unique_function<void(uWS::WebSocket<SSL, true> *, HttpRequest *)> open = nullptr;
        fu2::unique_function<void(uWS::WebSocket<SSL, true> *, std::string_view, uWS::OpCode)> message = nullptr;
        fu2::unique_function<void(uWS::WebSocket<SSL, true> *)> drain = nullptr;
        fu2::unique_function<void(uWS::WebSocket<SSL, true> *)> ping = nullptr;
        fu2::unique_function<void(uWS::WebSocket<SSL, true> *)> pong = nullptr;
        fu2::unique_function<void(uWS::WebSocket<SSL, true> *, int, std::string_view)> close = nullptr;
    };

    template <typename UserData>
    TemplatedApp &&ws(std::string pattern, WebSocketBehavior &&behavior) {
        /* Don't compile if alignment rules cannot be satisfied */
        static_assert(alignof(UserData) <= LIBUS_EXT_ALIGNMENT,
        "µWebSockets cannot satisfy UserData alignment requirements. You need to recompile µSockets with LIBUS_EXT_ALIGNMENT adjusted accordingly.");

        /* Every route has its own websocket context with its own behavior and user data type */
        auto *webSocketContext = WebSocketContext<SSL, true>::create(Loop::get(), (us_socket_context_t *) httpContext);

        /* We need to clear this later on */
        webSocketContexts.push_back(webSocketContext);

        /* Quick fix to disable any compression if set */
#ifdef UWS_NO_ZLIB
        behavior.compression = uWS::DISABLED;
#endif

        /* If we are the first one to use compression, initialize it */
        if (behavior.compression) {
            LoopData *loopData = (LoopData *) us_loop_ext(us_socket_context_loop(SSL, webSocketContext->getSocketContext()));

            /* Initialize loop's deflate inflate streams */
            if (!loopData->zlibContext) {
                loopData->zlibContext = new ZlibContext;
                loopData->inflationStream = new InflationStream;
                loopData->deflationStream = new DeflationStream;
            }
        }

        /* Copy all handlers */
        webSocketContext->getExt()->messageHandler = std::move(behavior.message);
        webSocketContext->getExt()->drainHandler = std::move(behavior.drain);
        webSocketContext->getExt()->closeHandler = std::move([closeHandler = std::move(behavior.close)](WebSocket<SSL, true> *ws, int code, std::string_view message) mutable {
            closeHandler(ws, code, message);

            /* Destruct user data after returning from close handler */
            ((UserData *) ws->getUserData())->~UserData();
        });

        /* Copy settings */
        webSocketContext->getExt()->maxPayloadLength = behavior.maxPayloadLength;
        webSocketContext->getExt()->idleTimeout = behavior.idleTimeout;
        webSocketContext->getExt()->maxBackpressure = behavior.maxBackpressure;

        httpContext->onHttp("get", pattern, [webSocketContext, httpContext = this->httpContext, behavior = std::move(behavior)](auto *res, auto *req) mutable {

            /* If we have this header set, it's a websocket */
            std::string_view secWebSocketKey = req->getHeader("sec-websocket-key");
            if (secWebSocketKey.length() == 24) {
                /* Note: OpenSSL can be used here to speed this up somewhat */
                char secWebSocketAccept[29] = {};
                WebSocketHandshake::generate(secWebSocketKey.data(), secWebSocketAccept);

                res->writeStatus("101 Switching Protocols")
                    ->writeHeader("Upgrade", "websocket")
                    ->writeHeader("Connection", "Upgrade")
                    ->writeHeader("Sec-WebSocket-Accept", secWebSocketAccept);

                /* Select first subprotocol if present */
                std::string_view secWebSocketProtocol = req->getHeader("sec-websocket-protocol");
                if (secWebSocketProtocol.length()) {
                    res->writeHeader("Sec-WebSocket-Protocol", secWebSocketProtocol.substr(0, secWebSocketProtocol.find(',')));
                }

                /* Negotiate compression */
                bool perMessageDeflate = false;
                bool slidingDeflateWindow = false;
                if (behavior.compression != DISABLED) {
                    std::string_view extensions = req->getHeader("sec-websocket-extensions");
                    if (extensions.length()) {
                        /* We never support client context takeover (the client cannot compress with a sliding window). */
                        int wantedOptions = PERMESSAGE_DEFLATE | CLIENT_NO_CONTEXT_TAKEOVER;

                        /* Shared compressor is the default */
                        if (behavior.compression == SHARED_COMPRESSOR) {
                            /* Disable per-socket compressor */
                            wantedOptions |= SERVER_NO_CONTEXT_TAKEOVER;
                        }

                        /* isServer = true */
                        ExtensionsNegotiator<true> extensionsNegotiator(wantedOptions);
                        extensionsNegotiator.readOffer(extensions);

                        /* Todo: remove these mid string copies */
                        std::string offer = extensionsNegotiator.generateOffer();
                        if (offer.length()) {
                            res->writeHeader("Sec-WebSocket-Extensions", offer);
                        }

                        /* Did we negotiate permessage-deflate? */
                        if (extensionsNegotiator.getNegotiatedOptions() & PERMESSAGE_DEFLATE) {
                            perMessageDeflate = true;
                        }

                        /* Is the server allowed to compress with a sliding window? */
                        if (!(extensionsNegotiator.getNegotiatedOptions() & SERVER_NO_CONTEXT_TAKEOVER)) {
                            slidingDeflateWindow = true;
                        }
                    }
                }

                /* This will add our mark */
                res->upgrade();

                /* Move any backpressure */
                std::string backpressure(std::move(((AsyncSocketData<SSL> *) res->getHttpResponseData())->buffer));

                /* Keep any fallback buffer alive until we returned from open event, keeping req valid */
                std::string fallback(std::move(res->getHttpResponseData()->salvageFallbackBuffer()));

                /* Destroy HttpResponseData */
                res->getHttpResponseData()->~HttpResponseData();

                /* Adopting a socket invalidates it, do not rely on it directly to carry any data */
                WebSocket<SSL, true> *webSocket = (WebSocket<SSL, true> *) us_socket_context_adopt_socket(SSL,
                            (us_socket_context_t *) webSocketContext, (us_socket_t *) res, sizeof(WebSocketData) + sizeof(UserData));

                /* Update corked socket in case we got a new one (assuming we always are corked in handlers). */
                webSocket->AsyncSocket<SSL>::cork();

                /* Initialize websocket with any moved backpressure intact */
                httpContext->upgradeToWebSocket(
                            webSocket->init(perMessageDeflate, slidingDeflateWindow, std::move(backpressure))
                            );

                /* Emit open event and start the timeout */
                if (behavior.open) {
                    us_socket_timeout(SSL, (us_socket_t *) webSocket, behavior.idleTimeout);

                    /* Default construct the UserData right before calling open handler */
                    new (webSocket->getUserData()) UserData;

                    behavior.open(webSocket, req);
                }

                /* We are going to get uncorked by the Http get return */

                /* We do not need to check for any close or shutdown here as we immediately return from get handler */

            } else {
                /* Tell the router that we did not handle this request */
                req->setYield(true);
            }
        }, true);
        return std::move(*this);
    }

    TemplatedApp &&get(std::string pattern, fu2::unique_function<void(HttpResponse<SSL> *, HttpRequest *)> &&handler) {
        httpContext->onHttp("get", pattern, std::move(handler));
        return std::move(*this);
    }

    TemplatedApp &&post(std::string pattern, fu2::unique_function<void(HttpResponse<SSL> *, HttpRequest *)> &&handler) {
        httpContext->onHttp("post", pattern, std::move(handler));
        return std::move(*this);
    }

    TemplatedApp &&options(std::string pattern, fu2::unique_function<void(HttpResponse<SSL> *, HttpRequest *)> &&handler) {
        httpContext->onHttp("options", pattern, std::move(handler));
        return std::move(*this);
    }

    TemplatedApp &&del(std::string pattern, fu2::unique_function<void(HttpResponse<SSL> *, HttpRequest *)> &&handler) {
        httpContext->onHttp("delete", pattern, std::move(handler));
        return std::move(*this);
    }

    TemplatedApp &&patch(std::string pattern, fu2::unique_function<void(HttpResponse<SSL> *, HttpRequest *)> &&handler) {
        httpContext->onHttp("patch", pattern, std::move(handler));
        return std::move(*this);
    }

    TemplatedApp &&put(std::string pattern, fu2::unique_function<void(HttpResponse<SSL> *, HttpRequest *)> &&handler) {
        httpContext->onHttp("put", pattern, std::move(handler));
        return std::move(*this);
    }

    TemplatedApp &&head(std::string pattern, fu2::unique_function<void(HttpResponse<SSL> *, HttpRequest *)> &&handler) {
        httpContext->onHttp("head", pattern, std::move(handler));
        return std::move(*this);
    }

    TemplatedApp &&connect(std::string pattern, fu2::unique_function<void(HttpResponse<SSL> *, HttpRequest *)> &&handler) {
        httpContext->onHttp("connect", pattern, std::move(handler));
        return std::move(*this);
    }

    TemplatedApp &&trace(std::string pattern, fu2::unique_function<void(HttpResponse<SSL> *, HttpRequest *)> &&handler) {
        httpContext->onHttp("trace", pattern, std::move(handler));
        return std::move(*this);
    }

    /* This one catches any method */
    TemplatedApp &&any(std::string pattern, fu2::unique_function<void(HttpResponse<SSL> *, HttpRequest *)> &&handler) {
        httpContext->onHttp("*", pattern, std::move(handler));
        return std::move(*this);
    }

    /* Host, port, callback */
    TemplatedApp &&listen(std::string host, int port, fu2::unique_function<void(us_listen_socket_t *)> &&handler) {
        if (!host.length()) {
            return listen(port, std::move(handler));
        }
        handler(httpContext->listen(host.c_str(), port, 0));
        return std::move(*this);
    }

    /* Host, port, options, callback */
    TemplatedApp &&listen(std::string host, int port, int options, fu2::unique_function<void(us_listen_socket_t *)> &&handler) {
        if (!host.length()) {
            return listen(port, options, std::move(handler));
        }
        handler(httpContext->listen(host.c_str(), port, options));
        return std::move(*this);
    }

    /* Port, callback */
    TemplatedApp &&listen(int port, fu2::unique_function<void(us_listen_socket_t *)> &&handler) {
        handler(httpContext->listen(nullptr, port, 0));
        return std::move(*this);
    }

    /* Port, options, callback */
    TemplatedApp &&listen(int port, int options, fu2::unique_function<void(us_listen_socket_t *)> &&handler) {
        handler(httpContext->listen(nullptr, port, options));
        return std::move(*this);
    }

    TemplatedApp &&run() {
        uWS::run();
        return std::move(*this);
    }

};

typedef TemplatedApp<false> App;
typedef TemplatedApp<true> SSLApp;

}

#endif // UWS_APP_H
