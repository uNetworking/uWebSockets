#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H

#include <string.h>
#include <string_view>
#include <utility>

// holds the header pointers and wrappers
struct HttpRequest {

    struct Header {
        char *key, *value;
        unsigned int keyLength, valueLength;

        operator bool() {
            return key;
        }
    };

    #define MAX_HEADERS 100
    Header headers[MAX_HEADERS];

    // UNSAFETY NOTE: assumes *end == '\r' (might unref end pointer)
    static char *getHeaders(char *buffer, char *end, struct Header *headers, size_t maxHeaders) {
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


    int consumePostPadded(char *data, int length) {
        char *cursor = data;

        if (cursor = getHeaders(data, data + length, headers, MAX_HEADERS)) {
            // strip out the initial stuff
            headers->valueLength = std::max<int>(0, headers->valueLength - 9);
        }

        return cursor - data;
    }

    void fenceRegion(char *data, int length) {
        data[length] = '\r';
    }

    Header getHeader(const char *key, size_t length) {
        if (headers) {
            for (Header *h = headers; *++h; ) {
                if (h->keyLength == length && !strncmp(h->key, key, length)) {
                    return *h;
                }
            }
        }
        return {nullptr, nullptr, 0, 0};
    }

    std::string_view getHeader(std::string_view header) {
        Header h = getHeader(header.data(), header.length());

        if (h.key) {
            return std::string_view(h.value, h.valueLength);
        }

        return std::string_view(nullptr, 0);
    }

    std::string_view getUrl() {
        return std::string_view(headers[0].value, headers[0].valueLength);
    }

};

#endif // HTTPREQUEST_H
