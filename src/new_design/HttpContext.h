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

// we should opaqeuly depend on HttpResponse
namespace uWS {
    template<bool>
    struct HttpResponse;
}

// we parse everything only in the context?

// would it be okay to depend on the context?

// we depend on libusockets
#include <libusockets.h>

template <bool SSL>
struct HttpContext {


private:
    HttpContext() = delete;

    // helper to get the socket context
    us_socket_context *getSocketContext() {
        return (us_socket_context *) this;
    }

    HttpContextData<SSL> *getSocketContextData() {
        return (HttpContextData<SSL> *) us_socket_context_ext(getSocketContext());
    }

public:

    static HttpContext *create(us_loop *loop) {

        // todo: inplace initialize the data struct!

        // todo: actually register handlers on the socket context with the behavior of HTTP! (take from HttpApp.h)

        return (HttpContext *) us_create_socket_context(loop, sizeof(HttpContextData<SSL>));
    }

    void free() {
        us_socket_context_free((us_socket_context *) this);
    }

    void onGet(std::string_view pattern, std::function<void(uWS::HttpResponse<SSL> *)> handler) {
        HttpContextData<SSL> *data = getSocketContextData();

        // add things to the router

        data->handler = handler;
    }

    void listen() {
        // simulate routing and parsing a request

        HttpContextData<SSL> *data = getSocketContextData();

        // we need to pass an actual socket! not nullptr!

        // simulare
        data->handler(nullptr);


    }

};

#endif // HTTPCONTEXT_H
