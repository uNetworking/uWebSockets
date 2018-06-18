#ifndef HTTP_H
#define HTTP_H

#include "libusockets.h"
#include <functional>

struct HttpRequest {

};

struct HttpSocket {
    int offset = 0;

    HttpSocket() {

    }

    std::function<std::pair<const char *, int>(int)> outStream;
    void stream(int length, decltype(outStream) stream);
};

#endif // HTTP_H
