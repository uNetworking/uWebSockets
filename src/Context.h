#ifndef CONTEXT_H
#define CONTEXT_H

#include "Hub.h"

#include "libusockets.h"
#include <type_traits>

// represents a mix of two us_socket contexts either in ssl or not (one for http, one for websocket)

// if you want to share the same openssl context between the two us socket ssl contexts? how?

#include "Http.h"

//#define SWAP_F [](auto a, auto b){ if constexpr(SSL) return a; else return b; }

namespace uWS {

// holds 4 protocols: server http, client http, server websocket, client websocket (for either SSL or not)
template <bool SSL>
class ContextBase {

protected:

    template <class A, class B>
    static typename std::conditional<SSL, A, B>::type *static_dispatch(A *a, B *b) {
        if constexpr(SSL) {
            return a;
        } else {
            return b;
        }
    }

    typedef typename std::conditional<SSL, us_ssl_socket_context, us_socket_context>::type SOCKET_CONTEXT_TYPE;

    struct Data {
        Data() {


        }

        std::function<void(HttpSocket<SSL> *)> onHttpConnection;
        std::function<void(HttpSocket<SSL> *)> onHttpDisconnection;
        std::function<void(HttpSocket<SSL> *, HttpRequest *)> onHttpRequest;
    } *data;

    // server protocols
    SOCKET_CONTEXT_TYPE *httpServerContext;
    SOCKET_CONTEXT_TYPE *webSocketServerContext;



    // client protocols

public:

    // the shared constructor
    void init(us_loop *loop) {

            new (data = (Data *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)(httpServerContext)) Data();

            // register shims
            static_dispatch(us_ssl_socket_context_on_open, us_socket_context_on_open)(httpServerContext, [](auto *s) {
                Data *data = (Data *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)(static_dispatch(us_ssl_socket_get_context, us_socket_get_context)(s));


                // here we need to construct a HTTP socket on the ext!



                // we always give pointers to us_socket?
                // same mistake as before?

                if (!data->onHttpConnection) {
                    return;
                }

                // note: this is VERY tricky to keep bug-free!
                // we could give the ext here, and skip all bugs?
                data->onHttpConnection((HttpSocket<SSL> *) s);
            });

            static_dispatch(us_ssl_socket_context_on_close, us_socket_context_on_close)(httpServerContext, [](auto *s) {
                Data *data = (Data *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)(static_dispatch(us_ssl_socket_get_context, us_socket_get_context)(s));


                    // we always give pointers to us_socket?
                    // same mistake as before?

                    if (!data->onHttpDisconnection) {
                        return;
                    }

                    // note: this is VERY tricky to keep bug-free!
                    // we could give the ext here, and skip all bugs?
                    data->onHttpDisconnection((HttpSocket<SSL> *) s);
            });

            static_dispatch(us_ssl_socket_context_on_data, us_socket_context_on_data)(httpServerContext, [](auto *s, char *data, int length) {

                Data *contextData = (Data *) static_dispatch(us_ssl_socket_context_ext, us_socket_context_ext)(static_dispatch(us_ssl_socket_get_context, us_socket_get_context)(s));





                    // a HttpSocket is basically the Http state and everything needed for it, we use it to parse the data and it knows about its context

                    //

                    HttpRequest req(data, length);
                    if (req.isComplete()) {
                        contextData->onHttpRequest((HttpSocket<SSL> *) s, &req);
                    } else {
                        std::cout << "Got chunked HTTP headers!" << std::endl;
                    }

                    // here we run the HTTP parser on this data



                    // we always give pointers to us_socket?
                    // same mistake as before?

                    // note: this is VERY tricky to keep bug-free!
                    // we could give the ext here, and skip all bugs?
                    //data->((HttpSocket *) s);
            });
        }

    // for server
    void listen(const char *host, int port, int options) {
        static_dispatch(us_ssl_socket_context_listen, us_socket_context_listen)(httpServerContext, host, port, options, sizeof(HttpSocket<SSL>));
    }

    // for server
    ContextBase &onHttpRequest(decltype(Data::onHttpRequest) handler) {
        data->onHttpRequest = handler;

        return *this;
    }

    // for client and server
    ContextBase &onWebSocketConnection() {
        return *this;
    }

    ~ContextBase() {
    }
};

class Context : public ContextBase<false> {

public:
    Context() {

        // we create the context ourselves here
        httpServerContext = us_create_socket_context(defaultHub.loop, sizeof(Data));

        // init from base
        init(defaultHub.loop);
    }
};

class SSLContext : public ContextBase<true> {

public:
    // denna finns i barnet SSLContext!
    SSLContext(us_ssl_socket_context_options options) {
        // we create the context ourselves here
        httpServerContext = us_create_ssl_socket_context(defaultHub.loop, sizeof(Data), options);

        // init from base
        init(defaultHub.loop);
    }

};

}

#endif // CONTEXT_H
