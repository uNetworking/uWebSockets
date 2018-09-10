#ifndef HTTPCONTEXT_H
#define HTTPCONTEXT_H

// a more Cish implementaion

#include <string_view>
#include <functional>

// we depend on loop I guess
// yes, the loop does not depend on us
#include "Loop.h"

// we of course depend on our own data type
#include "HttpContextData.h"
#include "HttpResponseData.h"

// we should opaqeuly depend on HttpResponse
namespace uWS {
    template<bool>
    struct HttpResponse;
}

// we parse everything only in the context?

// would it be okay to depend on the context?

// this basically should mean: libusockets wrapper
#include "StaticDispatch.h"

template <bool SSL>
struct HttpContext : StaticDispatch<SSL> {

    using SOCKET_CONTEXT_TYPE = typename StaticDispatch<SSL>::SOCKET_CONTEXT_TYPE;
    using SOCKET_TYPE = typename StaticDispatch<SSL>::SOCKET_TYPE;
    using StaticDispatch<SSL>::static_dispatch;

    static const int HTTP_IDLE_TIMEOUT_S = 10;

private:
    HttpContext() = delete;

    // helper to get the socket context
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

public:

    HttpContext<SSL> *init() {
        //new (data = (Data *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)(httpServerContext)) Data();

        static_dispatch(us_ssl_socket_context_on_open, us_socket_context_on_open)(getSocketContext(), [](auto *s, int is_client) {
            HttpContextData<SSL> *httpContextData = getSocketContextData(s);

            std::cout << "Opened http connection" << std::endl;

            static_dispatch(us_ssl_socket_timeout, us_socket_timeout)(s, HTTP_IDLE_TIMEOUT_S);

            new (static_dispatch(us_ssl_socket_ext, us_socket_ext)(s)) HttpResponseData<SSL>;

            return s;
        });

        static_dispatch(us_ssl_socket_context_on_close, us_socket_context_on_close)(getSocketContext(), [](auto *s) {
            HttpContextData<SSL> *httpContextData = getSocketContextData(s);

            ((HttpResponseData<SSL> *) static_dispatch(us_ssl_socket_ext, us_socket_ext)(s))->~HttpResponseData<SSL>();

            return s;
        });

        static_dispatch(us_ssl_socket_context_on_data, us_socket_context_on_data)(getSocketContext(), [](auto *s, char *data, int length) {
            HttpContextData<SSL> *httpContextData = getSocketContextData(s);

            HttpResponseData<SSL> *httpResponseData = (HttpResponseData<SSL> *) static_dispatch(us_ssl_socket_ext, us_socket_ext)(s);
            httpResponseData->consumePostPadded(data, length, s, [httpContextData](void *s, HttpRequest *httpRequest) {

                // warning: if we are in shutdown state, resetting the timer is a security issue!
                static_dispatch(us_ssl_socket_timeout, us_socket_timeout)((SOCKET_TYPE *) s, HTTP_IDLE_TIMEOUT_S);

                // todo: route this according to our router

                httpContextData->handler((uWS::HttpResponse<SSL> *) s, httpRequest);

            }, [httpResponseData](void *user, std::string_view data) {
                if (httpResponseData->readHandler) {
                    httpResponseData->readHandler(data);
                }
            }, [](void *user) {
                std::cout << "INVALID HTTP!" << std::endl;

                // close it down
            });

            return s;
        });

        static_dispatch(us_ssl_socket_context_on_writable, us_socket_context_on_writable)(getSocketContext(), [](auto *s) {

            // what if the client

            // I think it's fair to never mind this one -> if we keep writing data after shutting down then that's an issue for us
            static_dispatch(us_ssl_socket_timeout, us_socket_timeout)(s, HTTP_IDLE_TIMEOUT_S);


            // why? WE should emit this event via the socket data that we access!
            ((HttpSocket<SSL> *) s)->onWritable();

            return s;
        });

        static_dispatch(us_ssl_socket_context_on_end, us_socket_context_on_end)(getSocketContext(), [](auto *s) {
            std::cout << "Socket was half-closed!" << std::endl;

            return s;
        });

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

    static HttpContext *create(us_loop *loop) {

        HttpContext *httpContext = (HttpContext *) us_create_socket_context(loop, sizeof(HttpContextData<SSL>));

        new ((HttpContextData<SSL> *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)((SOCKET_CONTEXT_TYPE *) httpContext)) HttpContextData<SSL>();

        return httpContext->init();
    }

    void free() {
        static_dispatch(us_ssl_socket_context_free, us_socket_context_free)(getSocketContext());
    }

    void onGet(std::string_view pattern, std::function<void(uWS::HttpResponse<SSL> *, HttpRequest *)> handler) {
        HttpContextData<SSL> *data = getSocketContextData();

        // add things to the router

        data->handler = handler;
    }

    void listen(const char *host, int port, int options) {
        static_dispatch(us_ssl_socket_context_listen, us_socket_context_listen)(getSocketContext(), host, port, options, sizeof(HttpContextData<SSL>));
    }

};

#endif // HTTPCONTEXT_H
