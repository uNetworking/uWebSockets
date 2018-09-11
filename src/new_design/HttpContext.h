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
        return (SOCKET_CONTEXT_TYPE *) us_socket_get_context(s);
    }

    HttpContextData<SSL> *getSocketContextData() {
        return (HttpContextData<SSL> *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)(getSocketContext());
    }

    static HttpContextData<SSL> *getSocketContextData(SOCKET_TYPE *s) {
        return (HttpContextData<SSL> *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)(getSocketContext(s));
    }

    /* Init the HttpContext by registering libusockets event handlers */
    HttpContext<SSL> *init() {
        //new (data = (Data *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)(httpServerContext)) Data();

        /* Handle socket connections */
        static_dispatch(us_ssl_socket_context_on_open, us_socket_context_on_open)(getSocketContext(), [](auto *s, int is_client) {
            HttpContextData<SSL> *httpContextData = getSocketContextData(s);

            std::cout << "Opened http connection" << std::endl;

            static_dispatch(us_ssl_socket_timeout, us_socket_timeout)(s, HTTP_IDLE_TIMEOUT_S);

            new (static_dispatch(us_ssl_socket_ext, us_socket_ext)(s)) HttpResponseData<SSL>;

            return s;
        });

        /* Handle socket disconnections */
        static_dispatch(us_ssl_socket_context_on_close, us_socket_context_on_close)(getSocketContext(), [](auto *s) {
            HttpContextData<SSL> *httpContextData = getSocketContextData(s);

            ((HttpResponseData<SSL> *) static_dispatch(us_ssl_socket_ext, us_socket_ext)(s))->~HttpResponseData<SSL>();

            return s;
        });

        /* Handle HTTP data streams */
        static_dispatch(us_ssl_socket_context_on_data, us_socket_context_on_data)(getSocketContext(), [](auto *s, char *data, int length) {
            HttpContextData<SSL> *httpContextData = getSocketContextData(s);

            // cork this socket (move this to loop?)
            ((AsyncSocket<SSL> *) s)->cork();

            // pass this pointer to pointer along with the routing and change it if upgraded
            SOCKET_TYPE *returnedSocket = s;

            HttpResponseData<SSL> *httpResponseData = (HttpResponseData<SSL> *) static_dispatch(us_ssl_socket_ext, us_socket_ext)(s);
            httpResponseData->consumePostPadded(data, length, s, [httpContextData](void *s, uWS::HttpRequest *httpRequest) {

                // warning: if we are in shutdown state, resetting the timer is a security issue!
                static_dispatch(us_ssl_socket_timeout, us_socket_timeout)((SOCKET_TYPE *) s, HTTP_IDLE_TIMEOUT_S);

                // route it!
                typename uWS::HttpContextData<SSL>::UserData userData = {
                    (HttpResponse<SSL> *) s, httpRequest
                };
                httpContextData->router.route("get", 3, httpRequest->getUrl().data(), httpRequest->getUrl().length(), &userData);

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

            // I think it's fair to never mind this one -> if we keep writing data after shutting down then that's an issue for us
            static_dispatch(us_ssl_socket_timeout, us_socket_timeout)(s, HTTP_IDLE_TIMEOUT_S);

            AsyncSocket<SSL> *asyncSocket = (AsyncSocket<SSL> *) s;

            // get next chunk to send
            HttpResponseData<SSL> *httpResponseData = (HttpResponseData<SSL> *) asyncSocket->getExt();

            if (httpResponseData->outStream) {
                std::string_view chunk = httpResponseData->outStream(httpResponseData->offset);

                // send, including any buffered up
                asyncSocket->mergeDrain(chunk);
            } else {
                std::cout << "We did not have any outStream!" << std::endl;

                asyncSocket->mergeDrain(std::string_view(nullptr, 0));
            }


            return s;
        });

        /* Handle FIN, HTTP does not support half-closed sockets, so simply close */
        static_dispatch(us_ssl_socket_context_on_end, us_socket_context_on_end)(getSocketContext(), [](auto *s) {

            // static_dispatch(us_ssl_socket_close, us_socket_close)(s);
            AsyncSocket<SSL> *asyncSocket = (AsyncSocket<SSL> *) s;
            asyncSocket->close();

            return s;
        });

        /* Handle socket timeouts */
        static_dispatch(us_ssl_socket_context_on_timeout, us_socket_context_on_timeout)(getSocketContext(), [](auto *s) {

            if (static_dispatch(us_ssl_socket_is_shut_down, us_socket_is_shut_down)(s)) {
                std::cout << "Forcefully closing socket since shutdown was not answered in time" << std::endl;
                static_dispatch(us_ssl_socket_close, us_socket_close)(s);
            } else {
                std::cout << "Shutting down socket now" << std::endl;
                static_dispatch(us_ssl_socket_timeout, us_socket_timeout)(s, HTTP_IDLE_TIMEOUT_S);
                static_dispatch(us_ssl_socket_shutdown, us_socket_shutdown)(s);
            }

            return s;

        });

        return this;
    }

public:
    /* Construct a new HttpContext using specified loop */
    static HttpContext *create(us_loop *loop) {
        HttpContext *httpContext = (HttpContext *) us_create_socket_context(loop, sizeof(HttpContextData<SSL>));

        new ((HttpContextData<SSL> *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)((SOCKET_CONTEXT_TYPE *) httpContext)) HttpContextData<SSL>();
        return httpContext->init();
    }

    /* Destruct the HttpContext, it does not follow RAII */
    void free() {

        // call destructor!

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
    void listen(const char *host, int port, int options) {
        static_dispatch(us_ssl_socket_context_listen, us_socket_context_listen)(getSocketContext(), host, port, options, sizeof(HttpResponseData<SSL>));
    }
};

}

#endif // HTTPCONTEXT_H
