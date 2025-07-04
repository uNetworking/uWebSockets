#ifndef UWS_CLI_APP_H
#define UWS_CLI_APP_H

#include <string>
#include <charconv>
#include <string_view>

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
        const char *ssl_ciphers = nullptr;
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
struct TemplatedClientApp {
private:
    /* The app always owns at least one http context, but creates websocket contexts on demand */
    HttpContext<SSL> *httpContext;
    
    char webSocketKey[24];
    bool handshakeSent = false;

    int port;
    std::string path;
    std::string host;

public:

    TemplatedClientApp(SocketContextOptions options = {}) {
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
        /* Maximum socket lifetime in minutes before forced closure (defaults to disabled) */
        unsigned short maxLifetime = 0;
        MoveOnlyFunction<void(HttpResponse<SSL> *, HttpRequest *, struct us_socket_context_t *)> upgrade = nullptr;
        MoveOnlyFunction<void(WebSocket<SSL, true, UserData> *)> open = nullptr;
        MoveOnlyFunction<void(WebSocket<SSL, true, UserData> *, std::string_view, OpCode)> message = nullptr;
        MoveOnlyFunction<void(WebSocket<SSL, true, UserData> *, std::string_view, OpCode)> dropped = nullptr;
        MoveOnlyFunction<void(WebSocket<SSL, true, UserData> *)> drain = nullptr;
        MoveOnlyFunction<void(WebSocket<SSL, true, UserData> *, std::string_view)> ping = nullptr;
        MoveOnlyFunction<void(WebSocket<SSL, true, UserData> *, std::string_view)> pong = nullptr;
        MoveOnlyFunction<void(WebSocket<SSL, true, UserData> *, std::string_view, int, int)> subscription = nullptr;
        MoveOnlyFunction<void(WebSocket<SSL, true, UserData> *, int, std::string_view)> close = nullptr;
    };

    template <typename UserData>
    TemplatedClientApp &&ws(WebSocketBehavior<UserData> &&behavior) {
        /* Don't compile if alignment rules cannot be satisfied */
        static_assert(alignof(UserData) <= LIBUS_EXT_ALIGNMENT,
        "µWebSockets cannot satisfy UserData alignment requirements. You need to recompile µSockets with LIBUS_EXT_ALIGNMENT adjusted accordingly.");

        if (!httpContext) {
            return std::move(static_cast<TemplatedClientApp &&>(*this));
        }

        /* Terminate on misleading idleTimeout values */
        if (behavior.idleTimeout && behavior.idleTimeout < 8) {
            std::cerr << "Error: idleTimeout must be either 0 or greater than 8!" << std::endl;
            std::terminate();
        }

        /* Maximum idleTimeout is 16 minutes */
        if (behavior.idleTimeout > 240 * 4) {
            std::cerr << "Error: idleTimeout must not be greater than 960 seconds!" << std::endl;
            std::terminate();
        }

        /* Maximum maxLifetime is 4 hours */
        if (behavior.maxLifetime > 240) {
            std::cerr << "Error: maxLifetime must not be greater than 240 minutes!" << std::endl;
            std::terminate();
        }

        auto *webSocketContext = WebSocketContext<SSL, true, UserData>::create(Loop::get(), (us_socket_context_t *) httpContext, nullptr);

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
                loopData->inflationStream = new InflationStream(CompressOptions::DEDICATED_DECOMPRESSOR);
                loopData->deflationStream = new DeflationStream(CompressOptions::DEDICATED_COMPRESSOR);
            }
        }

        /* Copy all handlers */
        webSocketContext->getExt()->openHandler = std::move(behavior.open);
        webSocketContext->getExt()->messageHandler = std::move(behavior.message);
        webSocketContext->getExt()->droppedHandler = std::move(behavior.dropped);
        webSocketContext->getExt()->drainHandler = std::move(behavior.drain);
        webSocketContext->getExt()->subscriptionHandler = std::move(behavior.subscription);
        webSocketContext->getExt()->closeHandler = std::move(behavior.close);
        webSocketContext->getExt()->pingHandler = std::move(behavior.ping);
        webSocketContext->getExt()->pongHandler = std::move(behavior.pong);

        /* Copy settings */
        webSocketContext->getExt()->maxPayloadLength = behavior.maxPayloadLength;
        webSocketContext->getExt()->maxBackpressure = behavior.maxBackpressure;
        webSocketContext->getExt()->closeOnBackpressureLimit = behavior.closeOnBackpressureLimit;
        webSocketContext->getExt()->resetIdleTimeoutOnSend = behavior.resetIdleTimeoutOnSend;
        webSocketContext->getExt()->sendPingsAutomatically = behavior.sendPingsAutomatically;
        webSocketContext->getExt()->maxLifetime = behavior.maxLifetime;
        webSocketContext->getExt()->compression = behavior.compression;

        /* Calculate idleTimeoutCompnents */
        webSocketContext->getExt()->calculateIdleTimeoutCompnents(behavior.idleTimeout);

        /* 
         * Filter is called only when the connection opens (event = 1) or 
         * closes (event = -1) of the socket. So we set a filter to send 
         * the initial websocket handshake once the server connection is open. 
         */
        httpContext->filter([ this ](HttpResponse<SSL> *res, int event) {
            if (event == 1) {
                /* Connection established */

                if (!this->handshakeSent) {
                    this->handshakeSent = true;

                    /* Writes a new key to "this->webSocketKey" */
                    WebSocketHandshake::generateKey(this->webSocketKey);

                    res->writeInitHandshake(this->host, this->path, this->webSocketKey);
                }
            } else if (event == -1) {
                /* Connection closed */
                if (this->handshakeSent) {
                    this->handshakeSent = false;
                }
            }
        });
        
        /* Creates a "fake" route to handle the server initial handshake response. */
        httpContext->onHttp("HTTP/1.1", "101", [this, webSocketContext, behavior = std::move(behavior)](HttpResponse<SSL> *res, HttpRequest *req) mutable {
            
            /* If we have this header set, it's a websocket handshake response */
            std::string_view secWebSocketAccept = req->getHeader("sec-websocket-accept");
            if (secWebSocketAccept.length() != 28) {
                /* Tell the router that we did not handle this request */
                req->setYield(true);
                return;
            }
            
            char expectedSecWebSocketAccept[29] = {};
            WebSocketHandshake::generate(this->webSocketKey, expectedSecWebSocketAccept);
            if (memcmp(secWebSocketAccept.data(), expectedSecWebSocketAccept, 28) != 0) {
                /* Tell the router that we did not handle this request */
                req->setYield(true);
                return;
            }

            /* Emit upgrade handler */
            if (behavior.upgrade) {
                behavior.upgrade(res, req, (struct us_socket_context_t *) webSocketContext);
            } else {
                /* Default handler upgrades to WebSocket */
                res->template upgrade<UserData>(
                    {}, 
                    {}, 
                    req->getHeader("sec-websocket-protocol"), 
                    req->getHeader("sec-websocket-extensions"), 
                    (struct us_socket_context_t *) webSocketContext,
                    true
                );
            }

            /* We are going to get uncorked by the Http get return */

            /* We do not need to check for any close or shutdown here as we immediately return from get handler */

        }, true);
        
        return std::move(static_cast<TemplatedClientApp &&>(*this));
    }

    TemplatedClientApp &&connect(std::string url) {
        /* Parses the URL setting "host", "path" and "port" */
        port = SSL ? 443 : 80;
        if (url.rfind("wss://", 0) == 0) {
            url = url.substr(6);
        } else if (url.rfind("ws://", 0) == 0) {
            url = url.substr(5);
        }
        size_t slash = url.find('/');
        std::string hostPort = url.substr(0, slash);
        path = (slash != std::string::npos) ? url.substr(slash) : "/";
        size_t colon = hostPort.find(':');
        if (colon != std::string::npos) {
            host = hostPort.substr(0, colon);
            port = std::stoi(hostPort.substr(colon + 1));
        } else {
            host = hostPort;
        }

        std::cout << "SSL= " << SSL << std::endl;
        std::cout << "host= " << host << std::endl;
        std::cout << "port= " << port << std::endl;

        /* Connect the socket */
        httpContext->connect(host.c_str(), port, 0);

        return std::move(static_cast<TemplatedClientApp &&>(*this));
    }

    TemplatedClientApp &&run() {
        uWS::run();
        return std::move(static_cast<TemplatedClientApp &&>(*this));
    }
};

};

namespace uWS {
    typedef uWS::TemplatedClientApp<false> CliApp;
    typedef uWS::TemplatedClientApp<true> CliSSLApp;
}

#endif // UWS_CLI_APP_H