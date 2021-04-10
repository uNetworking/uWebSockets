/*
 * Authored by Alex Hultman, 2018-2020.
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
#include "PerMessageDeflate.h"

namespace uWS {

    /* This one matches us_socket_context_options_t but has default values */
    struct SocketContextOptions {
        const char *key_file_name = nullptr;
        const char *cert_file_name = nullptr;
        const char *passphrase = nullptr;
        const char *dh_params_file_name = nullptr;
        const char *ca_file_name = nullptr;
        int ssl_prefer_low_memory_usage = 0;

        /* Conversion operator used internally */
        operator struct us_socket_context_options_t() const {
            struct us_socket_context_options_t socket_context_options;
            memcpy(&socket_context_options, this, sizeof(SocketContextOptions));
            return socket_context_options;
        }
    };

    static_assert(sizeof(struct us_socket_context_options_t) == sizeof(SocketContextOptions), "Mismatching uSockets/uWebSockets ABI");

template <bool SSL>
struct TemplatedApp {
private:
    /* The app always owns at least one http context, but creates websocket contexts on demand */
    HttpContext<SSL> *httpContext;
    std::vector<WebSocketContext<SSL, true, int> *> webSocketContexts;

public:

    /* Server name */
    TemplatedApp &&addServerName(std::string hostname_pattern, SocketContextOptions options = {}) {

        us_socket_context_add_server_name(SSL, (struct us_socket_context_t *) httpContext, hostname_pattern.c_str(), options);
        return std::move(*this);
    }

    TemplatedApp &&removeServerName(std::string hostname_pattern) {

        us_socket_context_remove_server_name(SSL, (struct us_socket_context_t *) httpContext, hostname_pattern.c_str());
        return std::move(*this);
    }

    TemplatedApp &&missingServerName(MoveOnlyFunction<void(const char *hostname)> handler) {

        if (!constructorFailed()) {
            httpContext->getSocketContextData()->missingServerNameHandler = std::move(handler);

            us_socket_context_on_server_name(SSL, (struct us_socket_context_t *) httpContext, [](struct us_socket_context_t *context, const char *hostname) {

                /* This is the only requirements of being friends with HttpContextData */
                HttpContext<SSL> *httpContext = (HttpContext<SSL> *) context;
                httpContext->getSocketContextData()->missingServerNameHandler(hostname);
            });
        }

        return std::move(*this);
    }

    /* Returns the SSL_CTX of this app, or nullptr. */
    void *getNativeHandle() {
        return us_socket_context_get_native_handle(SSL, (struct us_socket_context_t *) httpContext);
    }

    /* Attaches a "filter" function to track socket connections/disconnections */
    void filter(MoveOnlyFunction<void(HttpResponse<SSL> *, int)> &&filterHandler) {
        httpContext->filter(std::move(filterHandler));
    }

    /* Publishes a message to all websocket contexts - conceptually as if publishing to the one single
     * TopicTree of this app (technically there are many TopicTrees, however the concept is that one
     * app has one conceptual Topic tree) */
    void publish(std::string_view topic, std::string_view message, OpCode opCode, bool compress = false) {
        for (auto *webSocketContext : webSocketContexts) {
            webSocketContext->getExt()->publish(topic, message, opCode, compress);
        }
    }

    /* Returns number of subscribers for this topic, or 0 for failure.
     * This function should probably be optimized a lot in future releases,
     * it could be O(1) with a hash map of fullnames and their counts. */
    unsigned int numSubscribers(std::string_view topic) {
        unsigned int subscribers = 0;

        for (auto *webSocketContext : webSocketContexts) {
            auto *webSocketContextData = webSocketContext->getExt();

            Topic *t = webSocketContextData->topicTree.lookupTopic(topic);
            if (t) {
                subscribers += t->subs.size();
            }
        }

        return subscribers;
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

    TemplatedApp(SocketContextOptions options = {}) {
        httpContext = HttpContext<SSL>::create(Loop::get(), options);
    }

    bool constructorFailed() {
        return !httpContext;
    }

    template <typename UserData>
    struct WebSocketBehavior {
        /* Disabled compression by default - probably a bad default */
        CompressOptions compression = DISABLED;
        /* Maximum message size we can receive */
        unsigned int maxPayloadLength = 16 * 1024;
        /* 2 minutes timeout is good */
        unsigned short idleTimeout = 120;
        /* 64kb backpressure is probably good */
        unsigned int maxBackpressure = 64 * 1024;
        bool closeOnBackpressureLimit = false;
        /* This one depends on kernel timeouts and is a bad default */
        bool resetIdleTimeoutOnSend = false;
        /* A good default, esp. for newcomers */
        bool sendPingsAutomatically = true;
        /* Maximum socket lifetime in seconds before forced closure (defaults to disabled) */
        unsigned short maxLifetime = 0;
        MoveOnlyFunction<void(HttpResponse<SSL> *, HttpRequest *, struct us_socket_context_t *)> upgrade = nullptr;
        MoveOnlyFunction<void(WebSocket<SSL, true, UserData> *)> open = nullptr;
        MoveOnlyFunction<void(WebSocket<SSL, true, UserData> *, std::string_view, OpCode)> message = nullptr;
        MoveOnlyFunction<void(WebSocket<SSL, true, UserData> *)> drain = nullptr;
        MoveOnlyFunction<void(WebSocket<SSL, true, UserData> *, std::string_view)> ping = nullptr;
        MoveOnlyFunction<void(WebSocket<SSL, true, UserData> *, std::string_view)> pong = nullptr;
        MoveOnlyFunction<void(WebSocket<SSL, true, UserData> *, int, std::string_view)> close = nullptr;
    };

    template <typename UserData>
    TemplatedApp &&ws(std::string pattern, WebSocketBehavior<UserData> &&behavior) {
        /* Don't compile if alignment rules cannot be satisfied */
        static_assert(alignof(UserData) <= LIBUS_EXT_ALIGNMENT,
        "µWebSockets cannot satisfy UserData alignment requirements. You need to recompile µSockets with LIBUS_EXT_ALIGNMENT adjusted accordingly.");

        if (!httpContext) {
            return std::move(*this);
        }

        /* Terminate on misleading idleTimeout values */
        if (behavior.idleTimeout && behavior.idleTimeout < 8) {
            std::cerr << "Error: idleTimeout must be either 0 or greater than 8!" << std::endl;
            std::terminate();
        }

        if (behavior.idleTimeout % 4) {
            std::cerr << "Warning: idleTimeout should be a multiple of 4!" << std::endl;
        }

        /* Every route has its own websocket context with its own behavior and user data type */
        auto *webSocketContext = WebSocketContext<SSL, true, UserData>::create(Loop::get(), (us_socket_context_t *) httpContext);

        /* Add all other WebSocketContextData to this new WebSocketContextData */
        for (WebSocketContext<SSL, true, int> *adjacentWebSocketContext : webSocketContexts) {
            webSocketContext->getExt()->adjacentWebSocketContextDatas.push_back(adjacentWebSocketContext->getExt());
        }

        /* Add this WebSocketContextData to all other WebSocketContextData */
        for (WebSocketContext<SSL, true, int> *adjacentWebSocketContext : webSocketContexts) {
            adjacentWebSocketContext->getExt()->adjacentWebSocketContextDatas.push_back((WebSocketContextData<SSL, int> *) webSocketContext->getExt());
        }

        /* We need to clear this later on */
        webSocketContexts.push_back((WebSocketContext<SSL, true, int> *) webSocketContext);

        /* Quick fix to disable any compression if set */
#ifdef UWS_NO_ZLIB
        behavior.compression = DISABLED;
#endif

        /* If we are the first one to use compression, initialize it */
        if (behavior.compression) {
            LoopData *loopData = (LoopData *) us_loop_ext(us_socket_context_loop(SSL, webSocketContext->getSocketContext()));

            /* Initialize loop's deflate inflate streams */
            if (!loopData->zlibContext) {
                loopData->zlibContext = new ZlibContext;
                loopData->inflationStream = new InflationStream;
                loopData->deflationStream = new DeflationStream(CompressOptions::DEDICATED_COMPRESSOR);
            }
        }

        /* Copy all handlers */
        webSocketContext->getExt()->openHandler = std::move(behavior.open);
        webSocketContext->getExt()->messageHandler = std::move(behavior.message);
        webSocketContext->getExt()->drainHandler = std::move(behavior.drain);
        webSocketContext->getExt()->closeHandler = std::move([closeHandler = std::move(behavior.close)](WebSocket<SSL, true, UserData> *ws, int code, std::string_view message) mutable {
            if (closeHandler) {
                closeHandler(ws, code, message);
            }

            /* Destruct user data after returning from close handler */
            ((UserData *) ws->getUserData())->~UserData();
        });
        webSocketContext->getExt()->pingHandler = std::move(behavior.ping);
        webSocketContext->getExt()->pongHandler = std::move(behavior.pong);

        /* Copy settings */
        webSocketContext->getExt()->maxPayloadLength = behavior.maxPayloadLength;
        webSocketContext->getExt()->maxBackpressure = behavior.maxBackpressure;
        webSocketContext->getExt()->closeOnBackpressureLimit = behavior.closeOnBackpressureLimit;
        webSocketContext->getExt()->resetIdleTimeoutOnSend = behavior.resetIdleTimeoutOnSend;
        webSocketContext->getExt()->sendPingsAutomatically = behavior.sendPingsAutomatically;
        webSocketContext->getExt()->compression = behavior.compression;

        /* Calculate idleTimeoutCompnents */
        webSocketContext->getExt()->calculateIdleTimeoutCompnents(behavior.idleTimeout);

        httpContext->onHttp("get", pattern, [webSocketContext, behavior = std::move(behavior)](auto *res, auto *req) mutable {

            /* If we have this header set, it's a websocket */
            std::string_view secWebSocketKey = req->getHeader("sec-websocket-key");
            if (secWebSocketKey.length() == 24) {

                /* Emit upgrade handler */
                if (behavior.upgrade) {
                    behavior.upgrade(res, req, (struct us_socket_context_t *) webSocketContext);
                } else {
                    /* Default handler upgrades to WebSocket */
                    std::string_view secWebSocketProtocol = req->getHeader("sec-websocket-protocol");
                    std::string_view secWebSocketExtensions = req->getHeader("sec-websocket-extensions");

                    res->template upgrade<UserData>({}, secWebSocketKey, secWebSocketProtocol, secWebSocketExtensions, (struct us_socket_context_t *) webSocketContext);
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

    TemplatedApp &&get(std::string pattern, MoveOnlyFunction<void(HttpResponse<SSL> *, HttpRequest *)> &&handler) {
        if (httpContext) {
            httpContext->onHttp("get", pattern, std::move(handler));
        }
        return std::move(*this);
    }

    TemplatedApp &&post(std::string pattern, MoveOnlyFunction<void(HttpResponse<SSL> *, HttpRequest *)> &&handler) {
        if (httpContext) {
            httpContext->onHttp("post", pattern, std::move(handler));
        }
        return std::move(*this);
    }

    TemplatedApp &&options(std::string pattern, MoveOnlyFunction<void(HttpResponse<SSL> *, HttpRequest *)> &&handler) {
        if (httpContext) {
            httpContext->onHttp("options", pattern, std::move(handler));
        }
        return std::move(*this);
    }

    TemplatedApp &&del(std::string pattern, MoveOnlyFunction<void(HttpResponse<SSL> *, HttpRequest *)> &&handler) {
        if (httpContext) {
            httpContext->onHttp("delete", pattern, std::move(handler));
        }
        return std::move(*this);
    }

    TemplatedApp &&patch(std::string pattern, MoveOnlyFunction<void(HttpResponse<SSL> *, HttpRequest *)> &&handler) {
        if (httpContext) {
            httpContext->onHttp("patch", pattern, std::move(handler));
        }
        return std::move(*this);
    }

    TemplatedApp &&put(std::string pattern, MoveOnlyFunction<void(HttpResponse<SSL> *, HttpRequest *)> &&handler) {
        if (httpContext) {
            httpContext->onHttp("put", pattern, std::move(handler));
        }
        return std::move(*this);
    }

    TemplatedApp &&head(std::string pattern, MoveOnlyFunction<void(HttpResponse<SSL> *, HttpRequest *)> &&handler) {
        if (httpContext) {
            httpContext->onHttp("head", pattern, std::move(handler));
        }
        return std::move(*this);
    }

    TemplatedApp &&connect(std::string pattern, MoveOnlyFunction<void(HttpResponse<SSL> *, HttpRequest *)> &&handler) {
        if (httpContext) {
            httpContext->onHttp("connect", pattern, std::move(handler));
        }
        return std::move(*this);
    }

    TemplatedApp &&trace(std::string pattern, MoveOnlyFunction<void(HttpResponse<SSL> *, HttpRequest *)> &&handler) {
        if (httpContext) {
            httpContext->onHttp("trace", pattern, std::move(handler));
        }
        return std::move(*this);
    }

    /* This one catches any method */
    TemplatedApp &&any(std::string pattern, MoveOnlyFunction<void(HttpResponse<SSL> *, HttpRequest *)> &&handler) {
        if (httpContext) {
            httpContext->onHttp("*", pattern, std::move(handler));
        }
        return std::move(*this);
    }

    /* Host, port, callback */
    TemplatedApp &&listen(std::string host, int port, MoveOnlyFunction<void(us_listen_socket_t *)> &&handler) {
        if (!host.length()) {
            return listen(port, std::move(handler));
        }
        handler(httpContext ? httpContext->listen(host.c_str(), port, 0) : nullptr);
        return std::move(*this);
    }

    /* Host, port, options, callback */
    TemplatedApp &&listen(std::string host, int port, int options, MoveOnlyFunction<void(us_listen_socket_t *)> &&handler) {
        if (!host.length()) {
            return listen(port, options, std::move(handler));
        }
        handler(httpContext ? httpContext->listen(host.c_str(), port, options) : nullptr);
        return std::move(*this);
    }

    /* Port, callback */
    TemplatedApp &&listen(int port, MoveOnlyFunction<void(us_listen_socket_t *)> &&handler) {
        handler(httpContext ? httpContext->listen(nullptr, port, 0) : nullptr);
        return std::move(*this);
    }

    /* Port, options, callback */
    TemplatedApp &&listen(int port, int options, MoveOnlyFunction<void(us_listen_socket_t *)> &&handler) {
        handler(httpContext ? httpContext->listen(nullptr, port, options) : nullptr);
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
