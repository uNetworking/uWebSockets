#ifndef HTTPSOCKET_UWS_H
#define HTTPSOCKET_UWS_H

#include "Socket.h"
#include <string>
// #include <experimental/string_view>

#include <iostream>

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

enum HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    PATCH,
    OPTIONS,
    HEAD,
    TRACE,
    CONNECT,
    INVALID
};

struct HttpRequest {
    Header *headers;
    Header getHeader(const char *key) {
        return getHeader(key, strlen(key));
    }

    HttpRequest(Header *headers = nullptr) : headers(headers) {}

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
        if (headers->key) {
            return *headers;
        }
        return {nullptr, nullptr, 0, 0};
    }

    HttpMethod getMethod() {
        if (!headers->key) {
            return INVALID;
        }
        switch (headers->keyLength) {
        case 3:
            if (!strncmp(headers->key, "get", 3)) {
                return GET;
            } else if (!strncmp(headers->key, "put", 3)) {
                return PUT;
            }
            break;
        case 4:
            if (!strncmp(headers->key, "post", 4)) {
                return POST;
            } else if (!strncmp(headers->key, "head", 4)) {
                return HEAD;
            }
            break;
        case 5:
            if (!strncmp(headers->key, "patch", 5)) {
                return PATCH;
            } else if (!strncmp(headers->key, "trace", 5)) {
                return TRACE;
            }
            break;
        case 6:
            if (!strncmp(headers->key, "delete", 6)) {
                return DELETE;
            }
            break;
        case 7:
            if (!strncmp(headers->key, "options", 7)) {
                return OPTIONS;
            } else if (!strncmp(headers->key, "connect", 7)) {
                return CONNECT;
            }
            break;
        }
        return INVALID;
    }
};

struct HttpResponse;

template <const bool isServer>
struct WIN32_EXPORT HttpSocket : private uS::Socket {
    struct Data : uS::SocketData {
        std::string httpBuffer;
        size_t contentLength = 0;

        // todo: limit these to only client, but who cares for now?
        std::string path;
        std::string host;
        std::string subprotocol;
        void *httpUser;

        // used to close sockets not sending requests in time
        bool missedDeadline = false;

        // list of responses to end, handed out
        HttpResponse *outstandingResponsesHead = nullptr;
        HttpResponse *outstandingResponsesTail = nullptr;
        HttpResponse *preAllocatedResponse = nullptr;

        Data(uS::SocketData *socketData) : uS::SocketData(*socketData) {}
    };

    using uS::Socket::getUserData;
    using uS::Socket::setUserData;
    using uS::Socket::getAddress;
    using uS::Socket::Address;

    uv_poll_t *getPollHandle() const {return p;}

    using uS::Socket::shutdown;
    using uS::Socket::close;

    void terminate() {
        onEnd(*this);
    }

    HttpSocket(uS::Socket s) : uS::Socket(s) {}

    typename HttpSocket::Data *getData() {
        return (HttpSocket::Data *) getSocketData();
    }

    bool upgrade(const char *secKey, const char *extensions,
                 size_t extensionsLength, const char *subprotocol,
                 size_t subprotocolLength, bool *perMessageDeflate);

private:
    friend class uS::Socket;
    friend struct HttpResponse;
    friend struct Hub;
    static void onData(uS::Socket s, char *data, int length);
    static void onEnd(uS::Socket s);
};

struct HttpResponse {

    HttpSocket<true> httpSocket;
    HttpResponse *next = nullptr;
    void *userData = nullptr;
    void *extraUserData = nullptr;
    uS::SocketData::Queue::Message *messageQueue = nullptr;
    bool hasEnded = false;

    HttpResponse(HttpSocket<true> httpSocket) : httpSocket(httpSocket) {

    }

    template <bool isServer>
    static HttpResponse *allocateResponse(HttpSocket<isServer> httpSocket, typename HttpSocket<isServer>::Data *httpData) {
        if (httpData->preAllocatedResponse) {
            HttpResponse *ret = httpData->preAllocatedResponse;
            httpData->preAllocatedResponse = nullptr;
            return ret;
        } else {
            return new HttpResponse(httpSocket);
        }
    }

    //template <bool isServer>
    void freeResponse(typename HttpSocket<true>::Data *httpData) {
        if (httpData->preAllocatedResponse) {
            delete this;
        } else {
            httpData->preAllocatedResponse = this;
        }
    }

    // todo
    void write(const char *message, size_t length = 0,
               void(*callback)(void *httpSocket, void *data, bool cancelled, void *reserved) = nullptr,
               void *callbackData = nullptr) {

        struct NoopTransformer {
            static size_t estimate(const char *data, size_t length) {
                return length;
            }

            static size_t transform(const char *src, char *dst, size_t length, int transformData) {
                memcpy(dst, src, length);
                return length;
            }
        };

        httpSocket.sendTransformed<NoopTransformer>(message, length, callback, callbackData, 0);
    }

    void end(const char *message = nullptr, size_t length = 0,
             void(*callback)(void *httpResponse, void *data, bool cancelled, void *reserved) = nullptr,
             void *callbackData = nullptr) {

        struct TransformData {
            //ContentType contentType;
        } transformData;// = {uWS::ContentType::TEXT_HTML};

        struct HttpTransformer {
            static size_t estimate(const char *data, size_t length) {
                return length + 128;
            }

            static size_t transform(const char *src, char *dst, size_t length, TransformData transformData) {
                int offset = std::sprintf(dst, "HTTP/1.1 200 OK\r\nContent-Length: %u\r\n\r\n", (unsigned int) length);
                memcpy(dst + offset, src, length);
                return length + offset;
            }
        };

        if (httpSocket.getData()->outstandingResponsesHead != this) {
            uS::SocketData::Queue::Message *messagePtr = httpSocket.allocMessage(HttpTransformer::estimate(message, length));
            messagePtr->length = HttpTransformer::transform(message, (char *) messagePtr->data, length, transformData);
            messagePtr->callback = callback;
            messagePtr->callbackData = callbackData;
            messagePtr->nextMessage = messageQueue;
            messageQueue = messagePtr;
            hasEnded = true;
        } else {
            httpSocket.sendTransformed<HttpTransformer>(message, length, callback, callbackData, transformData);
            // move head as far as possible
            HttpResponse *head = next;
            while (head) {
                // empty message queue
                uS::SocketData::Queue::Message *messagePtr = head->messageQueue;
                while (messagePtr) {
                    uS::SocketData::Queue::Message *nextMessage = messagePtr->nextMessage;

                    bool wasTransferred;
                    if (httpSocket.write(messagePtr, wasTransferred)) {
                        if (!wasTransferred) {
                            httpSocket.freeMessage(messagePtr);
                            if (callback) {
                                callback(this, callbackData, false, nullptr);
                            }
                        } else {
                            messagePtr->callback = callback;
                            messagePtr->callbackData = callbackData;
                        }
                    } else {
                        httpSocket.freeMessage(messagePtr);
                        if (callback) {
                            callback(this, callbackData, true, nullptr);
                        }
                        goto updateHead;
                    }
                    messagePtr = nextMessage;
                }
                // cannot go beyond unfinished responses
                if (!head->hasEnded) {
                    break;
                } else {
                    HttpResponse *next = head->next;
                    head->freeResponse(httpSocket.getData());
                    head = next;
                }
            }
            updateHead:
            httpSocket.getData()->outstandingResponsesHead = head;
            if (!head) {
                httpSocket.getData()->outstandingResponsesTail = nullptr;
            }

            freeResponse(httpSocket.getData());
        }
    }

    void setUserData(void *userData) {
        this->userData = userData;
    }

    void *getUserData() {
        return userData;
    }

    HttpSocket<true> getHttpSocket() {
        return httpSocket;
    }
};

}

#endif // HTTPSOCKET_UWS_H
