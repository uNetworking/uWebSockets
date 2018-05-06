#ifndef HTTP_H
#define HTTP_H

#include "libusockets.h"

struct HttpRequest {

};

struct HttpResponse {

    void write() {

    }

};

#include <iostream>

// basically a HttpServer
struct HttpContext {
    us_socket_context *context;
    struct Data {

    } *data;

    static void httpBegin(us_socket *s) {
        // yep
        std::cout << "Accepted a connection" << std::endl;
    }

    static void httpData(us_socket *s, void *data, int size) {
        // yep
        std::cout << "got data" << std::endl;

        //us_loop_userdata(s->context->loop)

        // call into Hub::Data::onHttpRequest
        // us_loop_userdata(s->context->loop) = Hub::Data
    }

    static void httpEnd(us_socket *s) {
        // yep
        std::cout << "Disconnection of a connection" << std::endl;
    }

    HttpContext(us_loop *loop) {
        context = us_create_socket_context(loop, sizeof(us_socket_context));

        context->on_accepted = httpBegin;
        context->on_data = httpData;
        context->on_end = httpEnd;
    }

    void listen(const char *host, int port, int options) {
        us_socket_context_listen(context, host, port, options, sizeof(us_socket));
    }
};

#endif // HTTP_H
