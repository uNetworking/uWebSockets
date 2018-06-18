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
