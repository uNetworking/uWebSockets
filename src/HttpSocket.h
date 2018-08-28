#ifndef HTTP_H
#define HTTP_H

#include "libusockets.h"
#include "Loop.h"
#include "HttpParser.h"
#include <functional>
#include <cstring>
#include <algorithm>
#include <string>

template <bool SSL>
struct HttpSocket {

    const size_t MAX_FALLBACK_SIZE = 4096;

    template <class A, class B>
    static constexpr typename std::conditional<SSL, A, B>::type *static_dispatch(A *a, B *b) {
        if constexpr(SSL) {
            return a;
        } else {
            return b;
        }
    }

    typedef typename std::conditional<SSL, us_ssl_socket, us_socket>::type SOCKET_TYPE;

    int u32toa(uint32_t value, char *dst) {
        char temp[10];
        char *p = temp;
        do {
            *p++ = (char) (value % 10) + '0';
            value /= 10;
        } while (value > 0);

        int ret = p - temp;

        do {
            *dst++ = *--p;
        } while (p != temp);

        return ret;
    }

    // chunked response will be tricky with this buffering scheme
    // if we do not fit, we can always use the header buffer for this (both in and out!)
    // put first 8kb chunk in the http buffer, then from there it's the stream's job!
    // httpheaders should only have 1 stream in and 1 stream out, but we can have helper wrappers

    struct Data {
        HttpParser httpParser;

        std::function<void(std::string_view)> inStream;

        // out streaming (.end should be a wrapper of this!)
        int offset = 0;
        std::function<std::string_view(int)> outStream;
    };

    // only this one should be used!
    void writeToCorkBuffer(const char *src, int length) {
        uWS::Loop::Data *loopData = (uWS::Loop::Data *) us_loop_ext(us_socket_context_loop(us_socket_get_context((us_socket *) this)));

        memcpy(loopData->corkBuffer + loopData->corkOffset, src, length);
        loopData->corkOffset += length;
    }

    // never rely on this one!
    int writeToCorkBufferAndReset(const char *src, int length, int contentLength, bool expectMore) {
        uWS::Loop::Data *loopData = (uWS::Loop::Data *) us_loop_ext(us_socket_context_loop(us_socket_get_context((us_socket *) this)));

        memcpy(loopData->corkBuffer + loopData->corkOffset, "Content-Length: ", 16);
        loopData->corkOffset += 16;

        loopData->corkOffset += u32toa(contentLength, loopData->corkBuffer + loopData->corkOffset);

        memcpy(loopData->corkBuffer + loopData->corkOffset, "\r\n\r\n", 4);
        loopData->corkOffset += 4;


        memcpy(loopData->corkBuffer + loopData->corkOffset, src, length);
        loopData->corkOffset += length;

        int written = static_dispatch(us_ssl_socket_write, us_socket_write)((SOCKET_TYPE *) this, loopData->corkBuffer, loopData->corkOffset, expectMore);

        loopData->corkOffset = 0;
        return written;
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

    // this should not be anything other than a simple convenience wrapper of streams!
    void end(std::string_view data) {
        // end should not explicitly flush the cork buffer! delay to when done with all http data!
        writeToCorkBufferAndReset(data.data(), data.length(), data.length(), false);
    }

    // stream out (todo: fix up large sends and benchmark it again)
    void write(std::function<std::string_view(int)> cb, int length) {

        std::string_view chunk = cb(0);

        // kopiera upp till (SSL eller icke-ssl) max copy distance

        // om mer än detta, fortsätt skicka


        // this strategy can be simplified to one, we can even have MAX_COPY_DISTANCE_SSL and MAX_COPY_DISTANCE
        if (length < uWS::Loop::MAX_COPY_DISTANCE) {
            // what if the streamer cannot return any data?
            // then it should return something to pause write, and then start it again
            // basically we need throttling
            writeToCorkBufferAndReset(chunk.data(), chunk.length(), length, false);
        } else {
            // copying some data with the headers is a good idea for SSL but probably not for non-SSL
            writeToCorkBufferAndReset(chunk.data(), uWS::Loop::MAX_COPY_DISTANCE, length, true);

            // just assume this went fine
            Data *httpData = (Data *) static_dispatch(us_ssl_socket_ext, us_socket_ext)((SOCKET_TYPE *) this);

            // write that off!
            static_dispatch(us_ssl_socket_write, us_socket_write)((SOCKET_TYPE *) this, chunk.data() + uWS::Loop::MAX_COPY_DISTANCE, chunk.length() - uWS::Loop::MAX_COPY_DISTANCE, 0);

            // if offset is at the end, we are done
            if (httpData->offset < length) {
                httpData->outStream = cb;
            }
        }
    }

    // this thing should only be reachable from App!
    void onWritable() {
        Data *httpData = (Data *) static_dispatch(us_ssl_socket_ext, us_socket_ext)((SOCKET_TYPE *) this);


        // now we start streaming as much as possible in each call!
        std::string_view chunk = httpData->outStream(httpData->offset);

        // write that off!
        static_dispatch(us_ssl_socket_write, us_socket_write)((SOCKET_TYPE *) this, chunk.data(), chunk.length(), 0);
    }

    void onData(char *data, int length, std::function<void(HttpSocket<SSL> *, HttpRequest *)> &onHttpRequest) {
        Data *httpData = (Data *) static_dispatch(us_ssl_socket_ext, us_socket_ext)((SOCKET_TYPE *) this);

        // todo: this is where the HttpSocket binds together HttpParser and HttpRouter into one
        httpData->httpParser.consumePostPadded(data, length, this, [&onHttpRequest](void *user, HttpRequest *httpRequest) {
            onHttpRequest((HttpSocket<SSL> *) user, httpRequest);
        }, [httpData](void *user, std::string_view data) {
            if (httpData->inStream) {
                httpData->inStream(data);
            }
        }, [](void *user) {
            std::cout << "INVALID HTTP!" << std::endl;
        });
    }

    void read(decltype(Data::inStream) stream) {
        Data *httpData = (Data *) static_dispatch(us_ssl_socket_ext, us_socket_ext)((SOCKET_TYPE *) this);

        httpData->inStream = stream;
    }

    void close() {
        static_dispatch(us_ssl_socket_close, us_socket_close)((SOCKET_TYPE *) this);
    }

    HttpSocket() = delete;
};

#endif // HTTP_H
