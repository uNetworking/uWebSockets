#include "Http.h"
#include "Hub.h"

#include <iostream>

void HttpContext::httpBegin(us_socket *s) {
    Hub::Data *hubData = (Hub::Data *) us_loop_userdata(s->context->loop);

    //std::cout << "BeginHTTP: " << s << std::endl;

    // should not touch the socket?
    new (s) HttpSocket();

    // tester
    ((HttpSocket *) s)->outStream = nullptr;

    hubData->onHttpConnection((HttpSocket *) s);
}

void HttpContext::httpEnd(us_socket *s) {
    Hub::Data *hubData = (Hub::Data *) us_loop_userdata(s->context->loop);

    hubData->onHttpDisconnection((HttpSocket *) s);
}

void HttpContext::httpData(us_socket *s, void *data, int size) {
    Hub::Data *hubData = (Hub::Data *) us_loop_userdata(s->context->loop);

    hubData->onHttpRequest((HttpSocket *) s, nullptr, (char *) data, size);
}

void HttpContext::httpOut(us_socket *s) {
    HttpSocket *httpSocket = (HttpSocket *) s;

    std::pair<const char *, int> chunk = httpSocket->outStream(httpSocket->offset);
    httpSocket->offset += us_socket_write(s, chunk.first, chunk.second);
}

HttpContext::HttpContext() {
    context.on_accepted = httpBegin;
    context.on_data = httpData;
    context.on_end = httpEnd;
    context.on_writable = httpOut;
}

void HttpContext::listen(const char *host, int port, int options) {
    us_socket_context_listen(&context, host, port, options, sizeof(HttpSocket));
}

// HttpSocket
void HttpSocket::stream(int length, decltype(outStream) stream) {
    outStream = stream;
    offset = 0;

    // try stream
    std::pair<const char *, int> chunk = outStream(offset);
    offset = us_socket_write(&s, chunk.first, chunk.second);
}
