#ifndef HTTP_H
#define HTTP_H

#include "libusockets.h"
#include <functional>

struct HttpRequest {

};


// HttpSocket is the ext of us_socket
struct HttpSocket {

    // incomplete headers buffer
    std::string headerBuffer;

    //

    HttpSocket() {

    }

    // the HttpParser should maybe be moved out of this into its own HttpProtocol.h like with websocket?
    void parse(char *data, int length) {

        std::cout << "Parsing headers: " << std::string_view(data, length) << std::endl;

    }

    int offset = 0;

    std::function<std::pair<const char *, int>(int)> outStream;
    void stream(int length, decltype(outStream) stream);
};

#endif // HTTP_H
