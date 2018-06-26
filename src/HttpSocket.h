#ifndef HTTP_H
#define HTTP_H

#include "libusockets.h"
#include "Loop.h"
#include "HttpRequest.h"
#include <functional>
#include <cstring>
#include <algorithm>
#include <string>

template <bool SSL>
struct HttpSocket {

    const size_t MAX_FALLBACK_SIZE = 1024 * 4;

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

    int str2int(const char *str, int len) {
        int i;
        int ret = 0;
        for (i = 0; i < len; i++) {
            ret = ret * 10 + (str[i] - '0');
        }
        return ret;
    }

    // chunked response will be tricky with this buffering scheme
    // if we do not fit, we can always use the header buffer for this (both in and out!)
    // put first 8kb chunk in the http buffer, then from there it's the stream's job!
    // httpheaders should only have 1 stream in and 1 stream out, but we can have helper wrappers

    // data is stored in ext
    struct Data {
        // fallback buffering
        std::string fallback;

        // these two control input streaming
        int contentLength = 0;
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
    int writeToCorkBufferAndReset(const char *src, int length, int contentLength) {
        uWS::Loop::Data *loopData = (uWS::Loop::Data *) us_loop_ext(us_socket_context_loop(us_socket_get_context((us_socket *) this)));

        memcpy(loopData->corkBuffer + loopData->corkOffset, "Content-Length: ", 16);
        loopData->corkOffset += 16;

        loopData->corkOffset += u32toa(contentLength, loopData->corkBuffer + loopData->corkOffset);

        memcpy(loopData->corkBuffer + loopData->corkOffset, "\r\n\r\n", 4);
        loopData->corkOffset += 4;


        memcpy(loopData->corkBuffer + loopData->corkOffset, src, length);
        loopData->corkOffset += length;

        int written;

        if constexpr(SSL) {
            written = us_ssl_socket_write((us_ssl_socket *) this, loopData->corkBuffer, loopData->corkOffset);
        } else {
            written = us_socket_write((us_socket *) this, loopData->corkBuffer, loopData->corkOffset, 0);
        }

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
        writeToCorkBufferAndReset(data.data(), data.length(), data.length());
    }

    // stream out (todo: fix up large sends and benchmark it again)
    void write(std::function<std::string_view(int)> cb, int length) {

        std::string_view chunk = cb(0);

        if (length < uWS::Loop::MAX_COPY_DISTANCE) {
            // what if the streamer cannot return any data?
            // then it should return something to pause write, and then start it again
            // basically we need throttling
            writeToCorkBufferAndReset(chunk.data(), chunk.length(), length);
        } else {
            // basically finish off the header section and send it as separate syscall (we do not copy anthing in this strategy)
            writeToCorkBufferAndReset(nullptr, 0, length);

            // just assume this went fine
            Data *httpData = (Data *) static_dispatch(us_ssl_socket_ext, us_socket_ext)((SOCKET_TYPE *) this);

            // write that off!
            if constexpr (SSL) {

            } else {
                httpData->offset = us_socket_write((SOCKET_TYPE *) this, chunk.data(), chunk.length(), 0);
            }

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
        if constexpr (SSL) {

        } else {
            httpData->offset += us_socket_write((SOCKET_TYPE *) this, chunk.data(), chunk.length(), 0);
        }
    }

    void onData(char *data, int length, std::function<void(HttpSocket<SSL> *, HttpRequest *)> &onHttpRequest) {
        Data *httpData = (Data *) static_dispatch(us_ssl_socket_ext, us_socket_ext)((SOCKET_TYPE *) this);

        //std::cout << std::string_view(data, length) << std::endl;

        HttpRequest req;

        req.fenceRegion(data, length);

        if (httpData->contentLength) {
            // at this point we reset the timeout timer
            if (httpData->contentLength >= length) {
                httpData->inStream(std::string_view(data, length));
                httpData->contentLength -= length;
                // no change to the socket here!
                return;
            } else {
                httpData->inStream(std::string_view(data, httpData->contentLength));

                data += httpData->contentLength;
                length -= httpData->contentLength;

                httpData->contentLength = 0;
            }
        } else if (httpData->fallback.length()) {
            int maxCopyDistance = std::min(MAX_FALLBACK_SIZE - httpData->fallback.length(), (size_t) length);

            httpData->fallback.reserve(maxCopyDistance + 32); // padding should be same as libus
            httpData->fallback.append(data, maxCopyDistance);


            if (int consumed = req.consumePostPadded(httpData->fallback.data(), httpData->fallback.length()); consumed) {
                httpData->fallback.clear();

                data += consumed;
                length -= consumed;

                onHttpRequest(this, &req);

                // see if we can read any posted data here (do we have contentLength header set?)
            } else {
                if (httpData->fallback.length() < MAX_FALLBACK_SIZE) {
                    std::cout << "Http headers coming in chunks I see, fine I'll pass!" << std::endl;
                } else {
                    // here we failed to parse any header in the 4kb we were given!
                    std::cout << "INVALID HTTP! no more chances!" << std::endl;
                }
                // no change in socket, or a closed socket!
                return;
            }
        }

        for (int consumed = 0; length && (consumed = req.consumePostPadded(data, length)); ) {

            //std::cout << "Parsing now <" << std::string_view(data, length) << ">" << std::endl;

            data += consumed;
            length -= consumed;

            // first emit the request
            onHttpRequest(this, &req);

            // then consume and stream any data!
            if (std::string_view contentLength = req.getHeader("content-length"); contentLength.length()) {
                // can we read everything off right now?

                httpData->contentLength = str2int(contentLength.data(), contentLength.length());

                //std::cout << "content length!" << std::endl;


                int emittable = std::min(httpData->contentLength, length);


                //std::cout << "Emittable: " << emittable << std::endl;

                httpData->inStream(std::string_view(data, emittable));


                httpData->contentLength -= emittable;


                length -= emittable;

                // otherwise, enter contentLength state!
            } else {
                //std::cout << "We don't have content-length!" << std::endl;
            }

        }

        if (length) {
            if (length < MAX_FALLBACK_SIZE) {
                // buffer up for next
            } else {
                // invalid http!
                std::cout << "invalid http! fuck off!" << std::endl;
            }
        }
    }

    void read(decltype(Data::inStream) stream) {
        Data *httpData = (Data *) static_dispatch(us_ssl_socket_ext, us_socket_ext)((SOCKET_TYPE *) this);

        httpData->inStream = stream;
    }

    HttpSocket() = delete;
};

#endif // HTTP_H
