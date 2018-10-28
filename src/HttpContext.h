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

#ifndef HTTPCONTEXT_H
#define HTTPCONTEXT_H

/* This class defines the main behavior of HTTP and emits various events */

#include "Loop.h"
#include "HttpContextData.h"

#include "HttpResponseData.h"
#include "AsyncSocket.h"
#include "StaticDispatch.h"

#include <string_view>
#include <functional>

/* This is a hack for now on, update uSockets */
extern "C" int us_internal_socket_is_closed(struct us_socket *s);

namespace uWS {
template<bool> struct HttpResponse;

template <bool SSL>
struct HttpContext : StaticDispatch<SSL> {
private:
    using SOCKET_CONTEXT_TYPE = typename StaticDispatch<SSL>::SOCKET_CONTEXT_TYPE;
    using SOCKET_TYPE = typename StaticDispatch<SSL>::SOCKET_TYPE;
    using StaticDispatch<SSL>::static_dispatch;
    HttpContext() = delete;

    /* Maximum delay allowed until an HTTP connection is terminated due to outstanding request or rejected data (slow loris protection) */
    static const int HTTP_IDLE_TIMEOUT_S = 10;

    SOCKET_CONTEXT_TYPE *getSocketContext() {
        return (SOCKET_CONTEXT_TYPE *) this;
    }

    static SOCKET_CONTEXT_TYPE *getSocketContext(SOCKET_TYPE *s) {
        return (SOCKET_CONTEXT_TYPE *) static_dispatch(us_ssl_socket_get_context, us_socket_get_context)(s);
    }

    HttpContextData<SSL> *getSocketContextData() {
        return (HttpContextData<SSL> *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)(getSocketContext());
    }

    static HttpContextData<SSL> *getSocketContextDataS(SOCKET_TYPE *s) {
        return (HttpContextData<SSL> *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)(getSocketContext(s));
    }

    /* Init the HttpContext by registering libusockets event handlers */
    HttpContext<SSL> *init() {
        /* Handle socket connections */
        static_dispatch(us_ssl_socket_context_on_open, us_socket_context_on_open)(getSocketContext(), [](auto *s, int is_client) {
            /* Any connected socket should timeout until it has a request */
            static_dispatch(us_ssl_socket_timeout, us_socket_timeout)(s, HTTP_IDLE_TIMEOUT_S);

            /* Init socket ext */
            new (static_dispatch(us_ssl_socket_ext, us_socket_ext)(s)) HttpResponseData<SSL>;

            return s;
        });

        /* Handle socket disconnections */
        static_dispatch(us_ssl_socket_context_on_close, us_socket_context_on_close)(getSocketContext(), [](auto *s) {
            /* Get socket ext */
            HttpResponseData<SSL> *httpResponseData = (HttpResponseData<SSL> *) static_dispatch(us_ssl_socket_ext, us_socket_ext)(s);

            std::cout << "Close event!" << std::endl;

            /* Signal broken HTTP request only if we have a pending request */
            if (httpResponseData->onAborted) {
                httpResponseData->onAborted();
            }

            // we might want to also signal the read stream?
            // smash onAborted together with read?

            /* Destruct socket ext */
            httpResponseData->~HttpResponseData<SSL>();

            return s;
        });

        /* Handle HTTP data streams */
        static_dispatch(us_ssl_socket_context_on_data, us_socket_context_on_data)(getSocketContext(), [](auto *s, char *data, int length) {

            // total overhead is about 210k down to 180k
            // ~210k req/sec is the original perf with write in data
            // ~200k req/sec is with cork and formatting
            // ~190k req/sec is with http parsing
            // ~180k - 190k req/sec is with varying routing

            HttpContextData<SSL> *httpContextData = getSocketContextDataS(s);

            /* Do not accept any data while in shutdown state */
            if (static_dispatch(us_ssl_socket_is_shut_down, us_socket_is_shut_down)((SOCKET_TYPE *) s)) {
                return s;
            }

            HttpResponseData<SSL> *httpResponseData = (HttpResponseData<SSL> *) static_dispatch(us_ssl_socket_ext, us_socket_ext)(s);

            /* Cork this socket */
            ((AsyncSocket<SSL> *) s)->cork();

            void *returnedSocket = httpResponseData->consumePostPadded(data, length, s, [httpContextData](void *s, uWS::HttpRequest *httpRequest) -> void * {
                /* For every request we reset the timeout and hang until user makes action */
                /* Warning: if we are in shutdown state, resetting the timer is a security issue! */
                static_dispatch(us_ssl_socket_timeout, us_socket_timeout)((SOCKET_TYPE *) s, 0);

                /* Reset httpResponse */
                HttpResponseData<SSL> *httpResponseData = (HttpResponseData<SSL> *) static_dispatch(us_ssl_socket_ext, us_socket_ext)((SOCKET_TYPE *) s);
                httpResponseData->offset = 0;
                httpResponseData->state = 0;

                /* Route the method and URL */
                httpContextData->router.route(httpRequest->getMethod(), httpRequest->getUrl(), {
                                                  (HttpResponse<SSL> *) s, httpRequest
                                              });

                /* Was the socket closed? */
                if (us_internal_socket_is_closed((struct us_socket *) s)) {
                    return nullptr;
                }

                /* We absolutely have to terminate parsing if shutdown */
                if (static_dispatch(us_ssl_socket_is_shut_down, us_socket_is_shut_down)((SOCKET_TYPE *) s)) {
                    return nullptr;
                }

                /* Was the socket upgraded? */
                // if (some global variable in this loop)

                /* Continue parsing */
                return s;

            }, [httpResponseData](void *user, std::string_view data) -> void * {
                if (httpResponseData->inStream) {
                    httpResponseData->inStream(data);

                    /* Was the socket closed? */
                    if (us_internal_socket_is_closed((struct us_socket *) user)) {
                        return nullptr;
                    }

                    /* We absolutely have to terminate parsing if shutdown */
                    if (static_dispatch(us_ssl_socket_is_shut_down, us_socket_is_shut_down)((SOCKET_TYPE *) user)) {
                        return nullptr;
                    }
                }
                return user;
            }, [](void *user) {
                 /* Close any socket on HTTP errors */
                static_dispatch(us_ssl_socket_close, us_socket_close)((SOCKET_TYPE *) user);
                return nullptr;
            });

            /* Only uncork still valid sockets */
            if (returnedSocket == s) {
                /* Timeout on uncork failure */
                auto [written, failed] = ((AsyncSocket<SSL> *) s)->uncork();
                if (failed) {
                    ((AsyncSocket<SSL> *) s)->timeout(HTTP_IDLE_TIMEOUT_S);
                }
            } else {
                // was this socket upgraded?
                std::cout << "Socket was closed or shut down in handler or maybe upgraded" << std::endl;
            }

            return s;
        });

        /* Handle HTTP write out (note: SSL_read may trigger this spuriously, the app need to handle spurious calls) */
        static_dispatch(us_ssl_socket_context_on_writable, us_socket_context_on_writable)(getSocketContext(), [](auto *s) {

            std::cout << "HttpContext::onWritable event fired!" << std::endl;

            /* We are now writable, so hang timeout again */
            static_dispatch(us_ssl_socket_timeout, us_socket_timeout)(s, 0);

            AsyncSocket<SSL> *asyncSocket = (AsyncSocket<SSL> *) s;
            HttpResponseData<SSL> *httpResponseData = (HttpResponseData<SSL> *) asyncSocket->getExt();

            /* Ask the developer to write data and return success (true) or failure (false), OR skip sending anything and return success (true). */
            if (httpResponseData->onWritable) {
                /* We expect the developer to return whether or not write was successful (true).
                 * If write was never called, the developer should still return true so that we may drain. */
                bool success = httpResponseData->onWritable(httpResponseData->offset);

                /* The developer indicated that their onWritable failed. */
                if (!success) {
                    /* Skip testing if we can drain anything since that might perform an extra syscall */
                    return s;
                }
            }

            /* Drain any socket buffer and timeout on failure */
            auto [written, failed] = asyncSocket->write(nullptr, 0, true, 0);
            if (failed) {
                asyncSocket->timeout(HTTP_IDLE_TIMEOUT_S);
            }

            return s;
        });

        /* Handle FIN, HTTP does not support half-closed sockets, so simply close */
        static_dispatch(us_ssl_socket_context_on_end, us_socket_context_on_end)(getSocketContext(), [](auto *s) {

            /* We do not care for half closed sockets */
            AsyncSocket<SSL> *asyncSocket = (AsyncSocket<SSL> *) s;
            return asyncSocket->close();

        });

        /* Handle socket timeouts, simply close them so to not confuse client with FIN */
        static_dispatch(us_ssl_socket_context_on_timeout, us_socket_context_on_timeout)(getSocketContext(), [](auto *s) {

            /* Force close rather than gracefully shutdown and risk confusing the client with a complete download */
            AsyncSocket<SSL> *asyncSocket = (AsyncSocket<SSL> *) s;
            return asyncSocket->close();

        });

        return this;
    }

public:
    /* Construct a new HttpContext using specified loop */
    static HttpContext *create(Loop *loop, us_ssl_socket_context_options *ssl_options = nullptr) {
        HttpContext *httpContext;

        if constexpr(SSL) {
            httpContext = (HttpContext *) us_create_ssl_socket_context((us_loop *) loop, sizeof(HttpContextData<SSL>), *ssl_options);
        } else {
            httpContext = (HttpContext *) us_create_socket_context((us_loop *) loop, sizeof(HttpContextData<SSL>));
        }

        if (!httpContext) {
            return nullptr;
        }

        /* Init socket context data */
        new ((HttpContextData<SSL> *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)((SOCKET_CONTEXT_TYPE *) httpContext)) HttpContextData<SSL>();
        return httpContext->init();
    }

    /* Destruct the HttpContext, it does not follow RAII */
    void free() {
        /* Destruct socket context data */
        HttpContextData<SSL> *httpContextData = getSocketContextData();
        httpContextData->~HttpContextData<SSL>();

        /* Free the socket context in whole */
        static_dispatch(us_ssl_socket_context_free, us_socket_context_free)(getSocketContext());
    }

    /* Register an HTTP route handler acording to URL pattern */
    void onHttp(std::string method, std::string pattern, std::function<void(uWS::HttpResponse<SSL> *, uWS::HttpRequest *)> handler) {
        HttpContextData<SSL> *httpContextData = getSocketContextData();

        httpContextData->router.add(method, pattern, [handler](typename HttpContextData<SSL>::RouterData user, std::pair<int, std::string_view *> params) {
            user.httpRequest->setParameters(params);
            handler(user.httpResponse, user.httpRequest);
        });
    }

    void onUnhandled(std::function<void(uWS::HttpResponse<SSL> *, uWS::HttpRequest *)> handler) {
        HttpContextData<SSL> *httpContextData = getSocketContextData();

        httpContextData->router.unhandled([handler](typename HttpContextData<SSL>::RouterData user, std::pair<int, std::string_view *> params) {
            handler(user.httpResponse, user.httpRequest);
        });
    }

    /* Listen to port using this HttpContext */
    us_listen_socket *listen(const char *host, int port, int options) {
        return static_dispatch(us_ssl_socket_context_listen, us_socket_context_listen)(getSocketContext(), host, port, options, sizeof(HttpResponseData<SSL>));
    }
};

}

#endif // HTTPCONTEXT_H
