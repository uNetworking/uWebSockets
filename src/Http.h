#ifndef HTTP_H
#define HTTP_H

#include "libusockets.h"
#include <functional>

struct HttpRequest {

};

struct HttpSocket {
    us_socket s;
    int offset = 0;

    HttpSocket() {

    }

    std::function<std::pair<const char *, int>(int)> outStream;
    void stream(int length, decltype(outStream) stream);
};

// basically a HttpServer
struct HttpContext {
    us_socket_context context;
    /*struct Data {

    } *data;*/

    static void httpBegin(us_socket *s);
    // httpIn
    static void httpData(us_socket *s, void *data, int size);
    static void httpOut(us_socket *s);
    static void httpEnd(us_socket *s);

    HttpContext();
    void listen(const char *host, int port, int options);
};

#endif // HTTP_H
