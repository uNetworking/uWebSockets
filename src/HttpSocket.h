#ifndef HTTP_H
#define HTTP_H

#include "libusockets.h"
#include "Loop.h"
#include <functional>
#include <cstring>

// HttpSocket is an alias for us_socket
template <bool SSL>
struct HttpSocket {

    // chunked response will be tricky with this buffering scheme
    // if we do not fit, we can always use the header buffer for this (both in and out!)
    // put first 8kb chunk in the http buffer, then from there it's the stream's job!
    // httpheaders should only have 1 stream in and 1 stream out, but we can have helper wrappers

    // data is stored in ext
    struct Data {
        // incomplete headers buffer
        std::string headerBuffer;
        int offset = 0;

        std::function<std::pair<const char *, int>(int)> outStream;
    };

    // only this one should be used!
    void writeToCorkBuffer(const char *src, int length) {
        uWS::Loop::Data *loopData = (uWS::Loop::Data *) us_loop_ext(us_socket_context_loop(us_socket_get_context((us_socket *) this)));

        memcpy(loopData->corkBuffer + loopData->corkOffset, src, length);
        loopData->corkOffset += length;
    }

    // sprintf is super slow, this one is a lot faster
    int u32toa_naive(uint32_t value, char *dst) {
        char temp[10];
        char *p = temp;
        do {
            *p++ = char(value % 10) + '0';
            value /= 10;
        } while (value > 0);

        int ret = p - temp;

        do {
            *dst++ = *--p;
        } while (p != temp);

        return ret;
    }

    // never rely on this one!
    void writeToCorkBufferAndReset(const char *src, int length) {
        uWS::Loop::Data *loopData = (uWS::Loop::Data *) us_loop_ext(us_socket_context_loop(us_socket_get_context((us_socket *) this)));

        memcpy(loopData->corkBuffer + loopData->corkOffset, "Content-Length: ", 16);
        loopData->corkOffset += 16;

        loopData->corkOffset += u32toa_naive(length, loopData->corkBuffer + loopData->corkOffset);

        memcpy(loopData->corkBuffer + loopData->corkOffset, "\r\n\r\n", 4);
        loopData->corkOffset += 4;


        memcpy(loopData->corkBuffer + loopData->corkOffset, src, length);
        loopData->corkOffset += length;

        if constexpr(SSL) {
            us_ssl_socket_write((us_ssl_socket *) this, loopData->corkBuffer, loopData->corkOffset);
        } else {
            us_socket_write((us_socket *) this, loopData->corkBuffer, loopData->corkOffset, 0);
        }

        loopData->corkOffset = 0;
    }

    HttpSocket *writeStatus(std::string_view status) {
        writeToCorkBuffer("HTTP/1.1 ", 9);
        writeToCorkBuffer(status.data(), status.length());
        writeToCorkBuffer("\r\n", 2);
        return this;
    }

    HttpSocket *writeHeader(std::string_view key, std::string_view value) {
        writeToCorkBuffer(key.data(), key.length());
        writeToCorkBuffer(": ", 2);
        writeToCorkBuffer(value.data(), value.length());
        writeToCorkBuffer("\r\n", 2);
        return this;
    }

    void end(std::string_view data) {
        // end should not explicitly flush the cork buffer! delay to when done with all http data!
        writeToCorkBufferAndReset(data.data(), data.length());
    }

    // stream out (todo: fix up large sends and benchmark it again)
    void write(std::function<std::string_view(int)> cb, int length) {
        std::string_view chunk = cb(0);

        writeToCorkBufferAndReset(chunk.data(), chunk.length());
    }

    // can't create this!
    HttpSocket() = delete;

    // the HttpParser should maybe be moved out of this into its own HttpProtocol.h like with websocket?
    void parse(char *data, int length) {

        std::cout << "Parsing headers: " << std::string_view(data, length) << std::endl;

    }

    void stream(int length, decltype(Data::outStream) stream);
};

#endif // HTTP_H
