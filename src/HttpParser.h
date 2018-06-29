#ifndef HTTPPARSER_H
#define HTTPPARSER_H

// this whole header needs fixing and testing as a separate module, fuzzing testing

#include <string>
#include <functional>
#include <cstring>

struct HttpRequest {

    struct Header {
        char *key, *value;
        unsigned int keyLength, valueLength;

        operator bool() {
            return key;
        }
    };

    #define MAX_HEADERS 50
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

    /*struct HttpRequest {
        // std::string_view getUrl()
        // std::string_view getHeader(std::string_view)
    };*/

    std::string_view getUrl() {
        return std::string_view(headers[0].value, headers[0].valueLength);
    }

};

class HttpParser {
private:
    std::string fallback;
    int remainingStreamingBytes = 0;

    const size_t MAX_FALLBACK_SIZE = 1024 * 4;

public:

    int str2int(const char *str, int len) {
        int i;
        int ret = 0;
        for (i = 0; i < len; i++) {
            ret = ret * 10 + (str[i] - '0');
        }
        return ret;
    }

    // think about better interface for this one. HttpParser::consumePostPadded<limit or not>(data, length, httpData, onHttpRequest)
    template <int LIMIT_TO_ONE_REQUEST>
    inline int fenceAndConsumePostPadded(char *data, int length, void *user, HttpRequest *req, std::function<void(void *, HttpRequest *)> &requestHandler, std::function<void(void *, std::string_view)> &dataHandler) {
        int ret = 0;

        int consumed = 0;
        req->fenceRegion(data, length);
        while (length && (consumed = req->consumePostPadded(data, length))) {
            data += consumed;
            length -= consumed;

            ret += consumed;

            requestHandler(user, req);
            if (std::string_view contentLengthString = req->getHeader("content-length"); contentLengthString.length()) {
                remainingStreamingBytes = str2int(contentLengthString.data(), contentLengthString.length());
                int emittable = std::min(remainingStreamingBytes, length);
                dataHandler(user, std::string_view(data, emittable));
                remainingStreamingBytes -= emittable;
                length -= emittable;

                ret += emittable;
            }

            if (LIMIT_TO_ONE_REQUEST) {
                break;
            }
        }
        return ret;
    }


    inline void consumePostPadded(char *data, int length, void *user, std::function<void(void *, HttpRequest *)> &&requestHandler, std::function<void(void *, std::string_view)> &&dataHandler, std::function<void(void *)> &&errorHandler) {

        HttpRequest req;

        if (remainingStreamingBytes) {
            // at this point we reset the timeout timer, we are streaming and we got a chunk

            if (remainingStreamingBytes >= length) {
                dataHandler(user, std::string_view(data, length));
                remainingStreamingBytes -= length;
                // no change to the socket here! we read all data in the buffer, return
                return;
            } else {
                dataHandler(user, std::string_view(data, remainingStreamingBytes));

                data += remainingStreamingBytes;
                length -= remainingStreamingBytes;

                remainingStreamingBytes = 0;

                // okay we are done with that, let's parse some more
            }
        } else if (fallback.length()) {
            int had = fallback.length();

            int maxCopyDistance = std::min(MAX_FALLBACK_SIZE - fallback.length(), (size_t) length);

            fallback.reserve(maxCopyDistance + 32); // padding should be same as libus
            fallback.append(data, maxCopyDistance);

            if (int consumed = fenceAndConsumePostPadded<true>(fallback.data(), fallback.length(), user, &req, requestHandler, dataHandler); consumed) {
                data += consumed - had;
                length -= consumed - had;
            } else {
                if (fallback.length() == MAX_FALLBACK_SIZE) {
                    // here we failed to parse any header in the 4kb we were given!
                    std::cout << "INVALID HTTP! no more chances!" << std::endl;
                }
                // no change in socket, or a closed socket!
                return;
            }
        }

        int consumed = fenceAndConsumePostPadded<false>(data, length, user, &req, requestHandler, dataHandler);
        data += consumed;
        length -= consumed;

        if (length) {
            if (length < MAX_FALLBACK_SIZE) {
                fallback.append(data, length);
            } else {
                // invalid http!
                std::cout << "invalid http! fuck off!" << std::endl;
            }
        }

        // vore najs om invalid http kunde hamna i samma ställe av koden!
    }
};

#endif // HTTPPARSER_H
