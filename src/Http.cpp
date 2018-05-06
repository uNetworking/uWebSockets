#include "Http.h"
#include "Hub.h"

#include <iostream>

void HttpContext::httpBegin(us_socket *s) {
    // yep
    std::cout << "Accepted a connection" << std::endl;
}

void HttpContext::httpData(us_socket *s, void *data, int size) {
    Hub::Data *hubData = (Hub::Data *) us_loop_userdata(s->context->loop);


    hubData->onHttpRequest(nullptr, nullptr, (char *) data, size);
}

void HttpContext::httpEnd(us_socket *s) {
    // yep
    std::cout << "Disconnection of a connection" << std::endl;
}

HttpContext::HttpContext(us_loop *loop) {
    context = us_create_socket_context(loop, sizeof(us_socket_context));

    context->on_accepted = httpBegin;
    context->on_data = httpData;
    context->on_end = httpEnd;
}

void HttpContext::listen(const char *host, int port, int options) {
    us_socket_context_listen(context, host, port, options, sizeof(us_socket));
}
