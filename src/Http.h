#ifndef HTTP_H
#define HTTP_H

#include "libusockets.h"
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

    // cork is stored in hub
    char *getCorkBuffer() {
        // context -> hub -> cork buffer

        us_socket *s = (us_socket *) this;

        // get the loop from the context!
        //return us_socket_context_loop(us_socket_get_context(s));


        return corkBuffer;
    }

    HttpSocket *writeStatus(int status) {
        char *corkBuffer = getCorkBuffer();

        char largeBuf[] = "HTTP/1.1 200 OK\r\nContent-Length: 512\r\n\r\n";

        memcpy(corkBuffer + corkOffset, largeBuf, sizeof(largeBuf) - 1);
        corkOffset += sizeof(largeBuf) - 1;

        return this;
    }

    HttpSocket *writeHeader(char *key, char *value) {
        return this;
    }

    void end(char *data, int length) {
        us_socket *s = (us_socket *) this;

        char *corkBuffer = getCorkBuffer();
        memcpy(corkBuffer + corkOffset, data, length);
        corkOffset += length;

        us_socket_write(s, corkBuffer, corkOffset, 0);
        corkOffset = 0;
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
