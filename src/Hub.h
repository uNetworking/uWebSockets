#ifndef HUB_H
#define HUB_H

#include "libusockets.h"
#include <functional>
#include <new>
#include <string_view>
#include <iostream>

#include "Http.h"
#include "Context.h"

struct Hub;

// maybe a Context is both TCP and SSL in one?
template <bool isServer, bool isSSL>
struct Context {

    Hub &hub;

    struct Data {
        Data() {


        }

        std::function<void(HttpSocket *)> onHttpConnection;
        std::function<void(HttpSocket *)> onHttpDisconnection;
        std::function<void(HttpSocket *, HttpRequest *)> onHttpRequest;
    } *data;

    us_socket_context *httpContext;

    Context(Hub &hub) : hub(hub) {

    }

    // one for each? ssl and non-ssl?
    void init(us_loop *loop) {
        httpContext = us_create_socket_context(loop, sizeof(Data));

        new (data = (Data *) us_socket_context_ext(httpContext)) Data();

        // register shims
        us_socket_context_on_open(httpContext, [](us_socket *s) {
            Data *data = (Data *) us_socket_context_ext(us_socket_get_context(s));


            // here we need to construct a HTTP socket on the ext!



            // we always give pointers to us_socket?
            // same mistake as before?

            if (!data->onHttpConnection) {
                return;
            }

            // note: this is VERY tricky to keep bug-free!
            // we could give the ext here, and skip all bugs?
            data->onHttpConnection((HttpSocket *) s);
        });

        us_socket_context_on_close(httpContext, [](us_socket *s) {
            Data *data = (Data *) us_socket_context_ext(us_socket_get_context(s));

            // we always give pointers to us_socket?
            // same mistake as before?

            if (!data->onHttpDisconnection) {
                return;
            }

            // note: this is VERY tricky to keep bug-free!
            // we could give the ext here, and skip all bugs?
            data->onHttpDisconnection((HttpSocket *) s);
        });

        us_socket_context_on_data(httpContext, [](us_socket *s, char *data, int length) {
            //Data *data = (Data *) us_socket_context_ext(us_socket_get_context(s));




            // a HttpSocket is basically the Http state and everything needed for it, we use it to parse the data and it knows about its context

            //



            // here we run the HTTP parser on this data

            std::cout << std::string_view(data, length) << std::endl;

            // we always give pointers to us_socket?
            // same mistake as before?

            // note: this is VERY tricky to keep bug-free!
            // we could give the ext here, and skip all bugs?
            //data->((HttpSocket *) s);
        });
    }

    void onHttpConnection(decltype(Data::onHttpConnection) handler) {
        data->onHttpConnection = handler;
    }
    void onHttpDisconnection(decltype(Data::onHttpDisconnection) handler) {
        data->onHttpDisconnection = handler;
    }
    Hub &onHttpRequest(decltype(Data::onHttpRequest) handler) {
        data->onHttpRequest = handler;

        return hub;
    }

    // this should only be enabled if we are server context!

    // should return Loop, not hub? or simply make it so that ALL contexts stem from some Hub? Hub is essentually the Loop abstraction? could work
    Hub &listen(const char *host, int port, int options) {
        us_socket_context_listen(httpContext, host, port, options, sizeof(HttpSocket));

        return hub;
    }
};

// are we SSL or regular hub?
struct Hub : Context<true, false> {
    us_loop *loop;

    using Context::onHttpConnection;
    using Context<true, false>::listen;

    struct Data {
        Data() {


        }

    } *data;

    static void wakeupCb(us_loop *loop) {

    }

    static void preCb(us_loop *loop) {

    }

    static void postCb(us_loop *loop) {

    }

    Hub() : loop(us_create_loop(wakeupCb, preCb, postCb, sizeof(Data))), Context<true, false>(*this) {
        new (data = (Data *) us_loop_ext(loop)) Data();

        Context<true, false>::init(loop);
    }

    void run() {
        us_loop_run(loop);
    }

    ~Hub() {

    }
};

namespace uWS {
thread_local Hub defaultHub;
}

#endif // HUB_H
