#include "HTTPSocket.h"
#include "Group.h"
#include "Extensions.h"
#include <cstdio>

#define MAX_HEADERS 100
#define MAX_HEADER_BUFFER_SIZE 4096
#define FORCE_SLOW_PATH false

#include <iostream>

namespace uWS {

// UNSAFETY NOTE: assumes *end == '\r' (might unref end pointer)
char *getHeaders(char *buffer, char *end, Header *headers, size_t maxHeaders) {
    for (unsigned int i = 0; i < maxHeaders; i++) {
        for (headers->key = buffer; (*buffer != ':') & (*buffer > 32); *(buffer++) |= 32);
        if (*buffer == '\r') {
            if ((buffer != end) & (buffer[1] == '\n') & (i > 0)) {
                headers->key = nullptr;
                return buffer + 2;
            } else {
                return nullptr;
            }
        } else {
            headers->keyLength = buffer - headers->key;
            for (buffer++; (*buffer == ':' || *buffer < 33) && *buffer != '\r'; buffer++);
            headers->value = buffer;
            buffer = (char *) memchr(buffer, '\r', end - buffer); //for (; *buffer != '\r'; buffer++);
            if (buffer /*!= end*/ && buffer[1] == '\n') {
                headers->valueLength = buffer - headers->value;
                buffer += 2;
                headers++;
            } else {
                return nullptr;
            }
        }
    }
    return nullptr;
}

// UNSAFETY NOTE: assumes 24 byte input length
static void base64(unsigned char *src, char *dst) {
    static const char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i = 0; i < 18; i += 3) {
        *dst++ = b64[(src[i] >> 2) & 63];
        *dst++ = b64[((src[i] & 3) << 4) | ((src[i + 1] & 240) >> 4)];
        *dst++ = b64[((src[i + 1] & 15) << 2) | ((src[i + 2] & 192) >> 6)];
        *dst++ = b64[src[i + 2] & 63];
    }
    *dst++ = b64[(src[18] >> 2) & 63];
    *dst++ = b64[((src[18] & 3) << 4) | ((src[19] & 240) >> 4)];
    *dst++ = b64[((src[19] & 15) << 2)];
    *dst++ = '=';
}

template <bool isServer>
void HttpSocket<isServer>::onData(uS::Socket s, char *data, int length) {
    HttpSocket httpSocket(s);
    HttpSocket::Data *httpData = httpSocket.getData();

    httpSocket.cork(true);

    if (httpData->contentLength) {
        httpData->missedDeadline = false;
        if (httpData->contentLength >= length) {
            getGroup<isServer>(s)->httpDataHandler(httpData->outstandingResponsesTail, data, length, httpData->contentLength -= length);
            return;
        } else {
            getGroup<isServer>(s)->httpDataHandler(httpData->outstandingResponsesTail, data, httpData->contentLength, 0);
            data += httpData->contentLength;
            length -= httpData->contentLength;
            httpData->contentLength = 0;
        }
    }

    if (FORCE_SLOW_PATH || httpData->httpBuffer.length()) {
        if (httpData->httpBuffer.length() + length > MAX_HEADER_BUFFER_SIZE) {
            httpSocket.onEnd(s);
            return;
        }

        httpData->httpBuffer.reserve(httpData->httpBuffer.length() + length + WebSocketProtocol<uWS::CLIENT>::CONSUME_POST_PADDING);
        httpData->httpBuffer.append(data, length);
        data = (char *) httpData->httpBuffer.data();
        length = httpData->httpBuffer.length();
    }

    char *end = data + length;
    char *cursor = data;
    *end = '\r';
    Header headers[MAX_HEADERS];
    do {
        char *lastCursor = cursor;
        if ((cursor = getHeaders(cursor, end, headers, MAX_HEADERS))) {
            HttpRequest req(headers);

            if (isServer) {
                headers->valueLength = std::max<int>(0, headers->valueLength - 9);
                httpData->missedDeadline = false;
                if (req.getHeader("upgrade", 7)) {
                    if (getGroup<SERVER>(s)->httpUpgradeHandler) {
                        getGroup<SERVER>(s)->httpUpgradeHandler(HttpSocket<isServer>(s), req);
                    } else {
                        Header secKey = req.getHeader("sec-websocket-key", 17);
                        Header extensions = req.getHeader("sec-websocket-extensions", 24);
                        Header subprotocol = req.getHeader("sec-websocket-protocol", 22);
                        bool perMessageDeflate;
                        if (secKey.valueLength == 24 && httpSocket.upgrade(secKey.value, extensions.value, extensions.valueLength,
                                                                           subprotocol.value, subprotocol.valueLength, &perMessageDeflate)) {
                            getGroup<SERVER>(s)->removeHttpSocket(s);
                            s.enterState<WebSocket<SERVER>>(new WebSocket<SERVER>::Data(perMessageDeflate, httpData));
                            getGroup<SERVER>(s)->addWebSocket(s);
                            s.cork(true);
                            getGroup<SERVER>(s)->connectionHandler(WebSocket<SERVER>(s), req);
                            s.cork(false);
                            delete httpData;
                        } else {
                            // note: not needed, we can let the poll catch any errors
                            httpSocket.onEnd(s);
                        }
                    }
                    return;
                } else {
                    if (getGroup<SERVER>(s)->httpRequestHandler) {

                        HttpResponse *res = HttpResponse::allocateResponse(httpSocket, httpData);
                        if (httpData->outstandingResponsesTail) {
                            httpData->outstandingResponsesTail->next = res;
                        } else {
                            httpData->outstandingResponsesHead = res;
                        }
                        httpData->outstandingResponsesTail = res;

                        Header contentLength;
                        if (req.getMethod() != HttpMethod::GET && (contentLength = req.getHeader("content-length", 14))) {
                            httpData->contentLength = atoi(contentLength.value);
                            size_t bytesToRead = std::min<int>(httpData->contentLength, end - cursor);
                            getGroup<SERVER>(s)->httpRequestHandler(res, req, cursor, bytesToRead, httpData->contentLength -= bytesToRead);
                            cursor += bytesToRead;
                        } else {
                            getGroup<SERVER>(s)->httpRequestHandler(res, req, nullptr, 0, 0);
                        }

                        if (s.isClosed() || s.isShuttingDown()) {
                            return;
                        }
                    } else {
                        httpSocket.onEnd(s);
                        return;
                    }
                }
            } else {
                if (req.getHeader("upgrade", 7)) {
                    s.enterState<WebSocket<CLIENT>>(new WebSocket<CLIENT>::Data(false, httpData));

                    httpSocket.cancelTimeout();
                    httpSocket.setUserData(httpData->httpUser);
                    getGroup<CLIENT>(s)->addWebSocket(s);
                    s.cork(true);
                    getGroup<CLIENT>(s)->connectionHandler(WebSocket<CLIENT>(s), req);
                    s.cork(false);

                    if (!(s.isClosed() || s.isShuttingDown())) {
                        WebSocketProtocol<CLIENT> *kws = (WebSocketProtocol<CLIENT> *) ((WebSocket<CLIENT>::Data *) s.getSocketData());
                        kws->consume(cursor, end - cursor, s);
                    }

                    delete httpData;
                } else {
                    httpSocket.onEnd(s);
                }
                return;
            }
        } else {
            if (!httpData->httpBuffer.length()) {
                if (length > MAX_HEADER_BUFFER_SIZE) {
                    httpSocket.onEnd(s);
                } else {
                    httpData->httpBuffer.append(lastCursor, end - lastCursor);
                }
            }
            return;
        }
    } while(cursor != end);

    httpSocket.cork(false);
    httpData->httpBuffer.clear();
}

// todo: make this into a transformer and make use of sendTransformed
template <bool isServer>
bool HttpSocket<isServer>::upgrade(const char *secKey, const char *extensions, size_t extensionsLength,
                                   const char *subprotocol, size_t subprotocolLength, bool *perMessageDeflate) {
    if (isServer) {
        *perMessageDeflate = false;
        std::string extensionsResponse;
        if (extensionsLength) {
            Group<isServer> *group = getGroup<isServer>(*this);
            ExtensionsNegotiator<uWS::SERVER> extensionsNegotiator(group->extensionOptions);
            extensionsNegotiator.readOffer(std::string(extensions, extensionsLength));
            extensionsResponse = extensionsNegotiator.generateOffer();
            if (extensionsNegotiator.getNegotiatedOptions() & PERMESSAGE_DEFLATE) {
                *perMessageDeflate = true;
            }
        }

        unsigned char shaInput[] = "XXXXXXXXXXXXXXXXXXXXXXXX258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        memcpy(shaInput, secKey, 24);
        unsigned char shaDigest[SHA_DIGEST_LENGTH];
        SHA1(shaInput, sizeof(shaInput) - 1, shaDigest);

        char upgradeBuffer[1024];
        memcpy(upgradeBuffer, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ", 97);
        base64(shaDigest, upgradeBuffer + 97);
        memcpy(upgradeBuffer + 125, "\r\n", 2);
        size_t upgradeResponseLength = 127;
        if (extensionsResponse.length()) {
            memcpy(upgradeBuffer + upgradeResponseLength, "Sec-WebSocket-Extensions: ", 26);
            memcpy(upgradeBuffer + upgradeResponseLength + 26, extensionsResponse.data(), extensionsResponse.length());
            memcpy(upgradeBuffer + upgradeResponseLength + 26 + extensionsResponse.length(), "\r\n", 2);
            upgradeResponseLength += 26 + extensionsResponse.length() + 2;
        }
        if (subprotocolLength) {
            memcpy(upgradeBuffer + upgradeResponseLength, "Sec-WebSocket-Protocol: ", 24);
            memcpy(upgradeBuffer + upgradeResponseLength + 24, subprotocol, subprotocolLength);
            memcpy(upgradeBuffer + upgradeResponseLength + 24 + subprotocolLength, "\r\n", 2);
            upgradeResponseLength += 24 + subprotocolLength + 2;
        }
        static char stamp[] = "Sec-WebSocket-Version: 13\r\nWebSocket-Server: uWebSockets\r\n\r\n";
        memcpy(upgradeBuffer + upgradeResponseLength, stamp, sizeof(stamp) - 1);
        upgradeResponseLength += sizeof(stamp) - 1;

        uS::SocketData::Queue::Message *messagePtr = allocMessage(upgradeResponseLength, upgradeBuffer);
        bool wasTransferred;
        if (write(messagePtr, wasTransferred)) {
            if (!wasTransferred) {
                freeMessage(messagePtr);
            } else {
                messagePtr->callback = nullptr;
            }
        } else {
            onEnd(*this);
            return false;
        }
    } else {
        std::string optionalSubprotocol;
        if (!getData()->subprotocol.empty()) {
            optionalSubprotocol = "Sec-WebSocket-Protocol: " + getData()->subprotocol + "\r\n";
        }
        std::string upgradeHeaderBuffer = std::string("GET /") + getData()->path + " HTTP/1.1\r\n"
                                                                                   "Upgrade: websocket\r\n"
                                                                                   "Connection: Upgrade\r\n"
                                                                                   "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"
                                                                                   "Host: " + getData()->host + "\r\n"
                                                                                   + optionalSubprotocol +
                                                                                   "Sec-WebSocket-Version: 13\r\n\r\n";

        uS::SocketData::Queue::Message *messagePtr = allocMessage(upgradeHeaderBuffer.length(), upgradeHeaderBuffer.data());
        bool wasTransferred;
        if (write(messagePtr, wasTransferred)) {
            if (!wasTransferred) {
                freeMessage(messagePtr);
            } else {
                messagePtr->callback = nullptr;
            }
        } else {
            onEnd(*this);
            return false;
        }
    }
    return true;
}

template <bool isServer>
void HttpSocket<isServer>::onEnd(uS::Socket s) {
    if (!s.isShuttingDown()) {
        // todo: not going to be set from Hub::upgrade!
        // Hub::upgrade can fail and call onEnd with a socket not in a group!
        // upgrade should always succeed!
        if (isServer) {
            getGroup<isServer>(s)->removeHttpSocket(HttpSocket<isServer>(s));
            getGroup<isServer>(s)->httpDisconnectionHandler(HttpSocket<isServer>(s));
        }
    } else {
        s.cancelTimeout();
    }

    Data *httpSocketData = (Data *) s.getSocketData();

    s.close();

    while (!httpSocketData->messageQueue.empty()) {
        uS::SocketData::Queue::Message *message = httpSocketData->messageQueue.front();
        if (message->callback) {
            message->callback(nullptr, message->callbackData, true, nullptr);
        }
        httpSocketData->messageQueue.pop();
    }

    while (httpSocketData->outstandingResponsesHead) {
        getGroup<isServer>(s)->httpCancelledRequestHandler(httpSocketData->outstandingResponsesHead);
        HttpResponse *next = httpSocketData->outstandingResponsesHead->next;
        delete httpSocketData->outstandingResponsesHead;
        httpSocketData->outstandingResponsesHead = next;
    }

    if (httpSocketData->preAllocatedResponse) {
        delete httpSocketData->preAllocatedResponse;
    }

    if (!isServer) {
        s.cancelTimeout();
        getGroup<CLIENT>(s)->errorHandler(httpSocketData->httpUser);
    }

    delete httpSocketData;
}

template struct HttpSocket<SERVER>;
template struct HttpSocket<CLIENT>;

}
