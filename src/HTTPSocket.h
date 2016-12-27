#ifndef HTTPSOCKET_UWS_H
#define HTTPSOCKET_UWS_H

#include "Socket.h"
#include <string>
// #include <experimental/string_view>

namespace uWS {

struct Header {
    char *key, *value;
    unsigned int keyLength, valueLength;

    operator bool() {
        return key;
    }

    // slow without string_view!
    std::string toString() {
        return std::string(value, valueLength);
    }
};

enum ContentType {
    TEXT_HTML
};

enum HTTPVerb {
    GET,
    POST,
    INVALID
};

struct HTTPRequest {
    Header *headers;
    Header getHeader(const char *key) {
        return getHeader(key, strlen(key));
    }

    HTTPRequest(Header *headers = nullptr) : headers(headers) {}

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
    Header getUrl() {
        if (headers) {
            return *headers;
        }
        return {nullptr, nullptr, 0, 0};
    }

    HTTPVerb getVerb() {
        if (headers->keyLength == 3 && !strncmp(headers->key, "get", 3)) {
            return GET;
        } else if (headers->keyLength == 4 && !strncmp(headers->key, "post", 4)) {
            return POST;
        }
        return INVALID;
    }
};

template <const bool isServer>
struct WIN32_EXPORT HTTPSocket : private uS::Socket {
    struct Data : uS::SocketData {
        std::string httpBuffer;
        size_t contentLength = 0;

        // todo: limit these to only client, but who cares for now?
        std::string path;
        std::string host;
        void *httpUser;

        Data(uS::SocketData *socketData) : uS::SocketData(*socketData) {}
    };

    uv_poll_t *getPollHandle() const {return p;}
    void respond(char *message, size_t length, ContentType contentType, void(*callback)(void *webSocket, void *data, bool cancelled, void *reserved) = nullptr, void *callbackData = nullptr);
    using uS::Socket::shutdown;
    using uS::Socket::close;

    HTTPSocket(uS::Socket s) : uS::Socket(s) {}

    typename HTTPSocket::Data *getData() {
        return (HTTPSocket::Data *) getSocketData();
    }

    bool upgrade(const char *secKey, const char *extensions,
                 size_t extensionsLength, const char *subprotocol,
                 size_t subprotocolLength, bool *perMessageDeflate);

private:
    friend class uS::Socket;
    friend struct Hub;
    static void onData(uS::Socket s, char *data, int length);
    static void onEnd(uS::Socket s);
};

}

#endif // HTTPSOCKET_UWS_H
