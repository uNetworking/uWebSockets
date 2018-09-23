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

namespace uWS {
template<bool> struct HttpResponse;

template <bool SSL>
struct HttpContext : StaticDispatch<SSL> {
private:
    using SOCKET_CONTEXT_TYPE = typename StaticDispatch<SSL>::SOCKET_CONTEXT_TYPE;
    using SOCKET_TYPE = typename StaticDispatch<SSL>::SOCKET_TYPE;
    using StaticDispatch<SSL>::static_dispatch;
    HttpContext() = delete;

    /* Maximum delay allowed until an HTTP connection is terminated due to outstanding request (slow loris protection) */
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

    static HttpContextData<SSL> *getSocketContextData(SOCKET_TYPE *s) {
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

            /* Signal broken out stream */
            if (httpResponseData->outStream) {
                httpResponseData->outStream(-1);
            }

            /* Signal broken in stream */
            if (httpResponseData->inStream) {
                httpResponseData->inStream(std::string_view(nullptr, 0));
            }

            /* Destruct socket ext */
            httpResponseData->~HttpResponseData<SSL>();

            return s;
        });

        /* Handle HTTP data streams */
        static_dispatch(us_ssl_socket_context_on_data, us_socket_context_on_data)(getSocketContext(), [](auto *s, char *data, int length) {
            HttpContextData<SSL> *httpContextData = getSocketContextData(s);

            /* Do not accept any data while in shutdown state */
            if (static_dispatch(us_ssl_socket_is_shut_down, us_socket_is_shut_down)((SOCKET_TYPE *) s)) {
                return s;
            }

            // basically, when getting a new request we need to disable timeouts and enter paused mode!

            /* Cork this socket */
            ((AsyncSocket<SSL> *) s)->cork();

            // pass this pointer to pointer along with the routing and change it if upgraded
            SOCKET_TYPE *returnedSocket = s;

            HttpResponseData<SSL> *httpResponseData = (HttpResponseData<SSL> *) static_dispatch(us_ssl_socket_ext, us_socket_ext)(s);
            httpResponseData->consumePostPadded(data, length, s, [httpContextData](void *s, uWS::HttpRequest *httpRequest) {

                // whenever we get (the first) request we should disable timeout?

                // warning: if we are in shutdown state, resetting the timer is a security issue!
                // todo: do not reset timer, disable it to allow hang requests!
                static_dispatch(us_ssl_socket_timeout, us_socket_timeout)((SOCKET_TYPE *) s, HTTP_IDLE_TIMEOUT_S);

                /* Reset httpResponse */
                HttpResponseData<SSL> *httpResponseData = (HttpResponseData<SSL> *) static_dispatch(us_ssl_socket_ext, us_socket_ext)((SOCKET_TYPE *) s);
                httpResponseData->offset = 0;
                httpResponseData->state = 0;

                // route it!
                typename uWS::HttpContextData<SSL>::UserData userData = {
                    (HttpResponse<SSL> *) s, httpRequest
                };
                httpContextData->router.route("get", 3, httpRequest->getUrl().data(), httpRequest->getUrl().length(), &userData);

                // here we can be closed and in shutdown?

            }, [httpResponseData](void *user, std::string_view data) {
                if (httpResponseData->inStream) {
                    httpResponseData->inStream(data);
                }
            }, [](void *user) {
                // close any socket on HTTP errors
                //static_dispatch(us_ssl_socket_close, us_socket_close)((SOCKET_TYPE *) user);
            });

            // uncork only if not closed
            ((AsyncSocket<SSL> *) s)->uncork();

            // how do we return a new socket here, from the http route?
            // maybe hold upgradedSocket in the loopData and return that?
            return s;
        });

        /* Handle HTTP write out */
        static_dispatch(us_ssl_socket_context_on_writable, us_socket_context_on_writable)(getSocketContext(), [](auto *s) {

            /* Silence any spurious writable events due to SSL_read failing to write */
            AsyncSocket<SSL> *asyncSocket = (AsyncSocket<SSL> *) s;
            HttpResponseData<SSL> *httpResponseData = (HttpResponseData<SSL> *) asyncSocket->getExt();
            if (httpResponseData->state & HttpResponseData<SSL>::HTTP_PAUSED_STREAM_OUT) {
                return s;
            }

            /* Writing data should reset the timeout */
            static_dispatch(us_ssl_socket_timeout, us_socket_timeout)(s, HTTP_IDLE_TIMEOUT_S);

            /* Are we already ended and just waiting for a drain / shutdown? */
            if (httpResponseData->state & HttpResponseData<SSL>::HTTP_ENDED_STREAM_OUT) {

                /* Try and send everything buffered up */
                asyncSocket->mergeDrain();

                /* If we succeed with drainage we can finally shut down */
                if (!asyncSocket->hasBuffer()) {
                    asyncSocket->shutdown();
                }

                /* Nothing here for us */
                return s;
            }

            if (httpResponseData->outStream) {
                /* Regular path, request more data */
                auto [msg_more, chunk] = httpResponseData->outStream(httpResponseData->offset);
                httpResponseData->offset += asyncSocket->mergeDrain(chunk);

                // todo: we should loop until we cannot send anymore just like we do in HttpResponse::write(stream)!
            } else {
                /* We can come here if we only have socket buffers to drain yet no attached stream */
                asyncSocket->mergeDrain();
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

    /* Register an HTTP GET route handler acording to URL pattern */
    void onGet(std::string pattern, std::function<void(uWS::HttpResponse<SSL> *, uWS::HttpRequest *)> handler) {
        HttpContextData<SSL> *httpContextData = getSocketContextData();

        httpContextData->router.add("get", pattern.c_str(), [handler](typename HttpContextData<SSL>::UserData *user, auto *args) {
            handler(user->httpResponse, user->httpRequest);
        });
    }

    /* Listen to port using this HttpContext */
    us_listen_socket *listen(const char *host, int port, int options) {
        return static_dispatch(us_ssl_socket_context_listen, us_socket_context_listen)(getSocketContext(), host, port, options, sizeof(HttpResponseData<SSL>));
    }
};

}

#endif // HTTPCONTEXT_H
