#ifndef HTTP_H
#define HTTP_H

#include "libusockets.h"

struct HttpRequest {

};

struct HttpResponse {

    void write() {

    }

};

// basically a HttpServer
struct HttpContext {
    us_socket_context *context;
    struct Data {

    } *data;

    static void httpBegin(us_socket *s);
    static void httpData(us_socket *s, void *data, int size);
    static void httpEnd(us_socket *s);

    HttpContext(us_loop *loop);
    void listen(const char *host, int port, int options);
};

#endif // HTTP_H
