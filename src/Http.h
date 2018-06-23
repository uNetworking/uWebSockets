#ifndef HTTP_H
#define HTTP_H

#include "libusockets.h"
#include "Loop.h"
#include <functional>
#include <cstring>

// holds the header pointers and wrappers
struct HttpRequest {

    struct Header {
        char *key, *value;
        unsigned int keyLength, valueLength;
    };

    #define MAX_HEADERS 100
    Header headers[MAX_HEADERS];

    // UNSAFETY NOTE: assumes *end == '\r' (might unref end pointer)
    char *getHeaders(char *buffer, char *end, struct Header *headers, size_t maxHeaders) {
        for (unsigned int i = 0; i < maxHeaders; i++) {
            for (headers->key = buffer; (*buffer != ':') & (*buffer > 32); *(buffer++) |= 32);
            if (*buffer == '\r') {
                if ((buffer != end) & (buffer[1] == '\n') & (i > 0)) {
                    headers->key = 0;
                    return buffer + 2;
                } else {
                    return 0;
                }
            } else {
                headers->keyLength = (unsigned int) (buffer - headers->key);
                for (buffer++; (*buffer == ':' || *buffer < 33) && *buffer != '\r'; buffer++);
                headers->value = buffer;
                buffer = (char *) memchr(buffer, '\r', end - buffer); //for (; *buffer != '\r'; buffer++);
                if (buffer /*!= end*/ && buffer[1] == '\n') {
                    headers->valueLength = (unsigned int) (buffer - headers->value);
                    buffer += 2;
                    headers++;
                } else {
                    return 0;
                }
            }
        }
        return 0;
    }

    std::string_view url;

    HttpRequest(char *data, int length) {
        // parse the shit
        data[length] = '\r';

        if (getHeaders(data, data + length, headers, MAX_HEADERS)) {

            headers->valueLength = std::max<int>(0, headers->valueLength - 9);

            // headers should really just be string_view from the start!
            url = std::string_view(headers[0].value, headers[0].valueLength);
        }
    }

    std::string_view getUrl() {
        return url;
    }

    bool isComplete() {
        return url.length();
    }

};


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

    // can't create this!
    HttpSocket() = delete;

    // the HttpParser should maybe be moved out of this into its own HttpProtocol.h like with websocket?
    void parse(char *data, int length) {

        std::cout << "Parsing headers: " << std::string_view(data, length) << std::endl;

    }

    void stream(int length, decltype(Data::outStream) stream);
};

#endif // HTTP_H
