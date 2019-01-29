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

#ifndef HTTPCONTEXT_H
#define HTTPCONTEXT_H

/* This class defines the main behavior of HTTP and emits various events */

#include "Loop.h"
#include "HttpContextData.h"

#include "HttpResponseData.h"
#include "AsyncSocket.h"

#include <string_view>
#include <functional>

#include "f2/function2.hpp"

namespace uWS {
template<bool> struct HttpResponse;

template <bool SSL>
struct HttpContext {
private:
    HttpContext() = delete;

    /* Maximum delay allowed until an HTTP connection is terminated due to outstanding request or rejected data (slow loris protection) */
    static const int HTTP_IDLE_TIMEOUT_S = 10;

    us_new_socket_context_t *getSocketContext() {
        return (us_new_socket_context_t *) this;
    }

    static us_new_socket_context_t *getSocketContext(us_new_socket_t *s) {
        return (us_new_socket_context_t *) us_new_socket_context(SSL, s);
    }

    HttpContextData<SSL> *getSocketContextData() {
        return (HttpContextData<SSL> *) us_new_socket_context_ext(SSL, getSocketContext());
    }

    static HttpContextData<SSL> *getSocketContextDataS(us_new_socket_t *s) {
        return (HttpContextData<SSL> *) us_new_socket_context_ext(SSL, getSocketContext(s));
    }

    /* Init the HttpContext by registering libusockets event handlers */
    HttpContext<SSL> *init() {
        /* Handle socket connections */
        us_new_socket_context_on_open(SSL, getSocketContext(), [](auto *s, int is_client) {
            /* Any connected socket should timeout until it has a request */
            us_new_socket_timeout(SSL, s, HTTP_IDLE_TIMEOUT_S);

            /* Init socket ext */
            new (us_new_socket_ext(SSL, s)) HttpResponseData<SSL>;

            /* Call filter */
            HttpContextData<SSL> *httpContextData = getSocketContextDataS(s);
            for (auto &f : httpContextData->filterHandlers) {
                f((HttpResponse<SSL> *) s, 1);
            }

            return s;
        });

        /* Handle socket disconnections */
        us_new_socket_context_on_close(SSL, getSocketContext(), [](auto *s) {
            /* Get socket ext */
            HttpResponseData<SSL> *httpResponseData = (HttpResponseData<SSL> *) us_new_socket_ext(SSL, s);

            /* Call filter */
            HttpContextData<SSL> *httpContextData = getSocketContextDataS(s);
            for (auto &f : httpContextData->filterHandlers) {
                f((HttpResponse<SSL> *) s, -1);
            }

            /* Signal broken HTTP request only if we have a pending request */
            if (httpResponseData->onAborted) {
                httpResponseData->onAborted();
            }

            /* Destruct socket ext */
            httpResponseData->~HttpResponseData<SSL>();

            return s;
        });

        /* Handle HTTP data streams */
        us_new_socket_context_on_data(SSL, getSocketContext(), [](auto *s, char *data, int length) {

            // total overhead is about 210k down to 180k
            // ~210k req/sec is the original perf with write in data
            // ~200k req/sec is with cork and formatting
            // ~190k req/sec is with http parsing
            // ~180k - 190k req/sec is with varying routing

            HttpContextData<SSL> *httpContextData = getSocketContextDataS(s);

            /* Do not accept any data while in shutdown state */
            if (us_new_socket_is_shut_down(SSL, (us_new_socket_t *) s)) {
                return s;
            }

            HttpResponseData<SSL> *httpResponseData = (HttpResponseData<SSL> *) us_new_socket_ext(SSL, s);

            /* Cork this socket */
            ((AsyncSocket<SSL> *) s)->cork();

            // clients need to know the cursor after http parse, not servers!
            // how far did we read then? we need to know to continue with websocket parsing data? or?
            void *returnedSocket = httpResponseData->consumePostPadded(data, length, s, [httpContextData](void *s, uWS::HttpRequest *httpRequest) -> void * {
                /* For every request we reset the timeout and hang until user makes action */
                /* Warning: if we are in shutdown state, resetting the timer is a security issue! */
                us_new_socket_timeout(SSL, (us_new_socket_t *) s, 0);

                /* Reset httpResponse */
                HttpResponseData<SSL> *httpResponseData = (HttpResponseData<SSL> *) us_new_socket_ext(SSL, (us_new_socket_t *) s);
                httpResponseData->offset = 0;

                /* Are we not ready for another request yet? Terminate the connection. */
                if (httpResponseData->state & HttpResponseData<SSL>::HTTP_RESPONSE_PENDING) {
                    us_new_socket_close(SSL, (us_new_socket_t *) s);
                    return nullptr;
                }

                /* Mark pending request and emit it */
                httpResponseData->state = HttpResponseData<SSL>::HTTP_RESPONSE_PENDING;

                /* Route the method and URL in two passes */
                typename HttpContextData<SSL>::RouterData routerData = {(HttpResponse<SSL> *) s, httpRequest};
                if (!httpContextData->router.route(httpRequest->getMethod(), httpRequest->getUrl(), routerData)) {
                    /* If first pass failed, we try and match by "any" method */
                    if (!httpContextData->router.route("*", httpRequest->getUrl(), routerData)) {
                        /* If second pass fail, we have to force close this socket as we have no handler for it */
                        us_new_socket_close(SSL, (us_new_socket_t *) s);
                        return nullptr;
                    }
                }

                /* First of all we need to check if this socket was deleted due to upgrade */
                if (httpContextData->upgradedWebSocket) {
                    /* Reset upgradedWebSocket before we return */
                    void *tmp = httpContextData->upgradedWebSocket;
                    httpContextData->upgradedWebSocket = nullptr;
                    return tmp;
                }

                /* Was the socket closed? */
                if (us_new_socket_is_closed(SSL, (struct us_new_socket_t *) s)) {
                    return nullptr;
                }

                /* We absolutely have to terminate parsing if shutdown */
                if (us_new_socket_is_shut_down(SSL, (us_new_socket_t *) s)) {
                    return nullptr;
                }

                /* Returning from a request handler without responding or attaching an onAborted handler is ill-use */
                if (!((HttpResponse<SSL> *) s)->hasResponded() && !httpResponseData->onAborted) {
                    /* Throw exception here? */
                    std::cerr << "Error: Returning from a request handler without responding or attaching an abort handler is forbidden!" << std::endl;
                    std::terminate();
                }

                /* Continue parsing */
                return s;

            }, [httpResponseData](void *user, std::string_view data, bool fin) -> void * {
                if (httpResponseData->inStream) {
                    httpResponseData->inStream(data, fin);

                    /* Was the socket closed? */
                    if (us_new_socket_is_closed(SSL, (struct us_new_socket_t *) user)) {
                        return nullptr;
                    }

                    /* We absolutely have to terminate parsing if shutdown */
                    if (us_new_socket_is_shut_down(SSL, (us_new_socket_t *) user)) {
                        return nullptr;
                    }
                }
                return user;
            }, [](void *user) {
                 /* Close any socket on HTTP errors */
                us_new_socket_close(SSL, (us_new_socket_t *) user);
                return nullptr;
            });

            // basically we need to uncork in all cases, except for nullptr
            if (returnedSocket != nullptr) {
                /* Timeout on uncork failure */
                auto [written, failed] = ((AsyncSocket<SSL> *) returnedSocket)->uncork();
                if (failed) {
                    // do we have the same timeout for websockets?
                    ((AsyncSocket<SSL> *) s)->timeout(HTTP_IDLE_TIMEOUT_S);
                }

                return (us_new_socket_t *) returnedSocket;
            }

            /* We cannot return nullptr to the underlying stack in any case */
            return s;
        });

        /* Handle HTTP write out (note: SSL_read may trigger this spuriously, the app need to handle spurious calls) */
        us_new_socket_context_on_writable(SSL, getSocketContext(), [](auto *s) {

            /* We are now writable, so hang timeout again */
            us_new_socket_timeout(SSL, s, 0);

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
        us_new_socket_context_on_end(SSL, getSocketContext(), [](auto *s) {

            /* We do not care for half closed sockets */
            AsyncSocket<SSL> *asyncSocket = (AsyncSocket<SSL> *) s;
            return asyncSocket->close();

        });

        /* Handle socket timeouts, simply close them so to not confuse client with FIN */
        us_new_socket_context_on_timeout(SSL, getSocketContext(), [](auto *s) {

            /* Force close rather than gracefully shutdown and risk confusing the client with a complete download */
            AsyncSocket<SSL> *asyncSocket = (AsyncSocket<SSL> *) s;
            return asyncSocket->close();

        });

        return this;
    }

public:
    /* Construct a new HttpContext using specified loop */
    static HttpContext *create(Loop *loop, us_new_socket_context_options_t options = {}) {
        HttpContext *httpContext;

        httpContext = (HttpContext *) us_new_create_socket_context(SSL, (us_loop *) loop, sizeof(HttpContextData<SSL>), options);

        if (!httpContext) {
            return nullptr;
        }

        /* Init socket context data */
        new ((HttpContextData<SSL> *) us_new_socket_context_ext(SSL, (us_new_socket_context_t *) httpContext)) HttpContextData<SSL>();
        return httpContext->init();
    }

    /* Destruct the HttpContext, it does not follow RAII */
    void free() {
        /* Destruct socket context data */
        HttpContextData<SSL> *httpContextData = getSocketContextData();
        httpContextData->~HttpContextData<SSL>();

        /* Free the socket context in whole */
        us_new_socket_context_free(SSL, getSocketContext());
    }

    void filter(fu2::unique_function<void(HttpResponse<SSL> *, int)> &&filterHandler) {
        getSocketContextData()->filterHandlers.emplace_back(std::move(filterHandler));
    }

    /* Register an HTTP route handler acording to URL pattern */
    void onHttp(std::string method, std::string pattern, fu2::unique_function<void(HttpResponse<SSL> *, HttpRequest *)> &&handler) {
        HttpContextData<SSL> *httpContextData = getSocketContextData();

        httpContextData->router.add(method, pattern, [handler = std::move(handler)](typename HttpContextData<SSL>::RouterData &user, std::pair<int, std::string_view *> params) mutable {
            user.httpRequest->setYield(false);
            user.httpRequest->setParameters(params);
            handler(user.httpResponse, user.httpRequest);

            /* If any handler yielded, the router will keep looking for a suitable handler. */
            if (user.httpRequest->getYield()) {
                return false;
            }
            return true;
        });
    }

    // this should not be public
    void upgradeToWebSocket(void *newSocket) {
        HttpContextData<SSL> *httpContextData = getSocketContextData();

        httpContextData->upgradedWebSocket = newSocket;
    }

    /* Listen to port using this HttpContext */
    us_listen_socket *listen(const char *host, int port, int options) {
        return us_new_socket_context_listen(SSL, getSocketContext(), host, port, options, sizeof(HttpResponseData<SSL>));
    }
};

}

#endif // HTTPCONTEXT_H
