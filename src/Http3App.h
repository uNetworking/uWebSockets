#include "App.h"

#include "Http3Response.h"
#include "Http3Request.h"
#include "Http3Context.h"

namespace uWS {

    struct H3App {
        Http3Context *http3Context;

        H3App(SocketContextOptions options = {}) {
            /* Create the http3 context */
            http3Context = Http3Context::create((us_loop_t *)Loop::get(), {});

            http3Context->init();
        }

        /* Disallow copying, only move */
        H3App(const H3App &other) = delete;

        H3App(H3App &&other) {
            /* Move HttpContext */
            http3Context = other.http3Context;
            other.http3Context = nullptr;
        }

        /* Host, port, callback */
        H3App &&listen(std::string host, int port, MoveOnlyFunction<void(us_listen_socket_t *)> &&handler) {
            if (!host.length()) {
                return listen(port, std::move(handler));
            }
            http3Context->listen();
            //handler(httpContext ? httpContext->listen(host.c_str(), port, 0) : nullptr);
            return std::move(*this);
        }

        /* Host, port, options, callback */
        H3App &&listen(std::string host, int port, int options, MoveOnlyFunction<void(us_listen_socket_t *)> &&handler) {
            if (!host.length()) {
                return listen(port, options, std::move(handler));
            }
            http3Context->listen();
            //handler(httpContext ? httpContext->listen(host.c_str(), port, options) : nullptr);
            return std::move(*this);
        }

        /* Port, callback */
        H3App &&listen(int port, MoveOnlyFunction<void(us_listen_socket_t *)> &&handler) {
            http3Context->listen();
            //handler(httpContext ? httpContext->listen(nullptr, port, 0) : nullptr);
            return std::move(*this);
        }

        /* Port, options, callback */
        H3App &&listen(int port, int options, MoveOnlyFunction<void(us_listen_socket_t *)> &&handler) {
            http3Context->listen();
            //handler(http3Context ? http3Context->listen(nullptr, port, options) : nullptr);
            return std::move(*this);
        }

        H3App &&get(std::string pattern, MoveOnlyFunction<void(Http3Response *, Http3Request *)> &&handler) {
            if (http3Context) {
                http3Context->onHttp("GET", pattern, std::move(handler));
            }
            return std::move(*this);
        }

        H3App &&post(std::string pattern, MoveOnlyFunction<void(Http3Response *, Http3Request *)> &&handler) {
            if (http3Context) {
                http3Context->onHttp("POST", pattern, std::move(handler));
            }
            return std::move(*this);
        }

        H3App &&options(std::string pattern, MoveOnlyFunction<void(Http3Response *, Http3Request *)> &&handler) {
            if (http3Context) {
                http3Context->onHttp("OPTIONS", pattern, std::move(handler));
            }
            return std::move(*this);
        }

        H3App &&del(std::string pattern, MoveOnlyFunction<void(Http3Response *, Http3Request *)> &&handler) {
            if (http3Context) {
                http3Context->onHttp("DELETE", pattern, std::move(handler));
            }
            return std::move(*this);
        }

        H3App &&patch(std::string pattern, MoveOnlyFunction<void(Http3Response *, Http3Request *)> &&handler) {
            if (http3Context) {
                http3Context->onHttp("PATCH", pattern, std::move(handler));
            }
            return std::move(*this);
        }

        H3App &&put(std::string pattern, MoveOnlyFunction<void(Http3Response *, Http3Request *)> &&handler) {
            if (http3Context) {
                http3Context->onHttp("PUT", pattern, std::move(handler));
            }
            return std::move(*this);
        }

        H3App &&head(std::string pattern, MoveOnlyFunction<void(Http3Response *, Http3Request *)> &&handler) {
            if (http3Context) {
                http3Context->onHttp("HEAD", pattern, std::move(handler));
            }
            return std::move(*this);
        }

        H3App &&connect(std::string pattern, MoveOnlyFunction<void(Http3Response *, Http3Request *)> &&handler) {
            if (http3Context) {
                http3Context->onHttp("CONNECT", pattern, std::move(handler));
            }
            return std::move(*this);
        }

        H3App &&trace(std::string pattern, MoveOnlyFunction<void(Http3Response *, Http3Request *)> &&handler) {
            if (http3Context) {
                http3Context->onHttp("TRACE", pattern, std::move(handler));
            }
            return std::move(*this);
        }

        /* This one catches any method */
        H3App &&any(std::string pattern, MoveOnlyFunction<void(Http3Response *, Http3Request *)> &&handler) {
            if (http3Context) {
                http3Context->onHttp("*", pattern, std::move(handler));
            }
            return std::move(*this);
        }

        void run() {
            uWS::Loop::get()->run();
        }
    };
}