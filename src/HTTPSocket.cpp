#include "HTTPSocket.h"
#include "Group.h"
#include "Extensions.h"
#include <cstdio>

#define MAX_HEADERS 100
#define MAX_HEADER_BUFFER_SIZE 4096
#define FORCE_SLOW_PATH false

#include <iostream>

namespace uWS {

// needs some more work and checking!
char *getHeaders(char *buffer, size_t length, Header *headers, size_t maxHeaders) {
    char *end = buffer + length;
    for (int i = 0; i < maxHeaders; i++) {
        headers->key = buffer;
        for (; *buffer != ':' && !isspace(*buffer) && *buffer != '\r'; buffer++) {
            *buffer = tolower(*buffer);
        }
        if (*buffer == '\r') {
            if (!(buffer + 1 < end && buffer[1] == '\n')) {
                return nullptr;
            }
        } else {
            headers->keyLength = buffer - headers->key;
            for (buffer++; *buffer == ':' || isspace(*buffer); buffer++);
            headers->value = buffer;
            for (; *buffer != '\r'; buffer++);
            headers->valueLength = buffer - headers->value;
            if (buffer + 1 < end && *++buffer == '\n') {
                ++buffer;
                headers++;
                continue;
            } else {
                return nullptr;
            }
        }
        headers->key = nullptr;
        return buffer + 2;
    }
    return nullptr;
}

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
void HTTPSocket<isServer>::onData(uS::Socket s, char *data, int length) {
    HTTPSocket httpSocket(s);
    HTTPSocket::Data *httpData = httpSocket.getData();

    char *httpBuffer = data;
    int httpLength = length;
    if (FORCE_SLOW_PATH || httpData->httpBuffer.length()) {
        if (httpData->httpBuffer.length() + length > MAX_HEADER_BUFFER_SIZE) {
            httpSocket.onEnd(s);
            return;
        }

        httpData->httpBuffer.reserve(httpData->httpBuffer.length() + WebSocketProtocol<uWS::CLIENT>::CONSUME_POST_PADDING);
        httpData->httpBuffer.append(data, length);
        httpBuffer = (char *) httpData->httpBuffer.data();
        httpLength = httpData->httpBuffer.length();
    }

    Header headers[MAX_HEADERS];
    char *httpBody;
    if ((httpBody = getHeaders(httpBuffer, httpLength, headers, MAX_HEADERS))) {
        HTTPRequest req(headers);

        if (isServer) {
            headers->valueLength = std::max(0, headers->valueLength - 9);
            if (req.getHeader("upgrade", 7)) {
                Header secKey = req.getHeader("sec-websocket-key");
                Header extensions = req.getHeader("sec-websocket-extensions");
                Header subprotocol = req.getHeader("sec-websocket-protocol");
                bool perMessageDeflate;
                if (secKey.valueLength == 24 && httpSocket.upgrade(secKey.value, extensions.value, extensions.valueLength,
                                                                   subprotocol.value, subprotocol.valueLength, &perMessageDeflate)) {
                    s.cancelTimeout();
                    s.enterState<WebSocket<SERVER>>(new WebSocket<SERVER>::Data(perMessageDeflate, httpData));

                    ((Group<SERVER> *) s.getSocketData()->nodeData)->addWebSocket(s);
                    s.cork(true);
                    ((Group<SERVER> *) s.getSocketData()->nodeData)->connectionHandler(WebSocket<SERVER>(s), req);
                    s.cork(false);
                    delete httpData;
                } else {
                    httpSocket.onEnd(s);
                }
            } else {
                if (((Group<SERVER> *) s.getSocketData()->nodeData)->httpRequestHandler) {
                    ((Group<SERVER> *) s.getSocketData()->nodeData)->httpRequestHandler(s, req);

                    Header contentLength = req.getHeader("content-length");
                    if (contentLength) {
                        httpData->contentLength = atoi(contentLength.value);

                        if (((Group<SERVER> *) s.getSocketData()->nodeData)->httpDataHandler) {
                            size_t availableBytes = (httpLength - (httpBody - httpBuffer));
                            ((Group<SERVER> *) s.getSocketData()->nodeData)->httpDataHandler(s, httpBody, availableBytes, httpData->contentLength - availableBytes);
                            httpData->contentLength -= availableBytes;
                        }
                    }

                    // if finns mer available bytes, fortsätt parsning från start!
                    return;
                } else {
                    httpSocket.onEnd(s);
                }
            }
        } else {
            if (req.getHeader("upgrade", 7)) {
                s.enterState<WebSocket<CLIENT>>(new WebSocket<CLIENT>::Data(false, httpData));

                httpSocket.cancelTimeout();
                httpSocket.setUserData(httpData->httpUser);
                ((Group<CLIENT> *) s.getSocketData()->nodeData)->addWebSocket(s);
                s.cork(true);
                ((Group<CLIENT> *) s.getSocketData()->nodeData)->connectionHandler(WebSocket<CLIENT>(s), req);
                s.cork(false);

                if (!(s.isClosed() || s.isShuttingDown())) {
                    WebSocketProtocol<CLIENT> *kws = (WebSocketProtocol<CLIENT> *) ((WebSocket<CLIENT>::Data *) s.getSocketData());
                    kws->consume(httpBody, (httpLength - (httpBody - httpBuffer)), s);
                }

                delete httpData;
            } else {
                httpSocket.onEnd(s);
            }
        }
    } else {
        httpData->httpBuffer.append(data, length);
    }
}

template <bool isServer>
void HTTPSocket<isServer>::respond(char *message, size_t length, ContentType contentType,
                                   void(*callback)(void *webSocket, void *data, bool cancelled, void *reserved), void *callbackData) {
    // assume we always respond with less than 128 byte header
    const int HEADER_LENGTH = 128;

    if (hasEmptyQueue()) {
        if (length + sizeof(uS::SocketData::Queue::Message) + HEADER_LENGTH <= uS::NodeData::preAllocMaxSize) {
            int memoryLength = length + sizeof(uS::SocketData::Queue::Message) + HEADER_LENGTH;
            int memoryIndex = getSocketData()->nodeData->getMemoryBlockIndex(memoryLength);

            uS::SocketData::Queue::Message *messagePtr = (uS::SocketData::Queue::Message *) getSocketData()->nodeData->getSmallMemoryBlock(memoryIndex);
            messagePtr->data = ((char *) messagePtr) + sizeof(uS::SocketData::Queue::Message);

            // shared code!
            int offset = std::sprintf((char *) messagePtr->data, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n", length);
            memcpy((char *) messagePtr->data + offset, message, length);
            messagePtr->length = length + offset;

            bool wasTransferred;
            if (write(messagePtr, wasTransferred)) {
                if (!wasTransferred) {
                    getSocketData()->nodeData->freeSmallMemoryBlock((char *) messagePtr, memoryIndex);
                    if (callback) {
                        callback(*this, callbackData, false, nullptr);
                    }
                } else {
                    messagePtr->callback = callback;
                    messagePtr->callbackData = callbackData;
                }
            } else {
                if (callback) {
                    callback(*this, callbackData, true, nullptr);
                }
            }
        } else {
            uS::SocketData::Queue::Message *messagePtr = allocMessage(length + HEADER_LENGTH);

            // shared code!
            int offset = std::sprintf((char *) messagePtr->data, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n", length);
            memcpy((char *) messagePtr->data + offset, message, length);
            messagePtr->length = length + offset;

            bool wasTransferred;
            if (write(messagePtr, wasTransferred)) {
                if (!wasTransferred) {
                    freeMessage(messagePtr);
                    if (callback) {
                        callback(*this, callbackData, false, nullptr);
                    }
                } else {
                    messagePtr->callback = callback;
                    messagePtr->callbackData = callbackData;
                }
            } else {
                if (callback) {
                    callback(*this, callbackData, true, nullptr);
                }
            }
        }
    } else {
        uS::SocketData::Queue::Message *messagePtr = allocMessage(length + HEADER_LENGTH);

        // shared code!
        int offset = std::sprintf((char *) messagePtr->data, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n", length);
        memcpy((char *) messagePtr->data + offset, message, length);
        messagePtr->length = length + offset;

        messagePtr->callback = callback;
        messagePtr->callbackData = callbackData;
        enqueue(messagePtr);
    }
}

template <bool isServer>
bool HTTPSocket<isServer>::upgrade(const char *secKey, const char *extensions, size_t extensionsLength,
                                   const char *subprotocol, size_t subprotocolLength, bool *perMessageDeflate) {
    if (isServer) {
        *perMessageDeflate = false;
        std::string extensionsResponse;
        if (extensionsLength) {
            Group<isServer> *group = (Group<isServer> *) getNodeData(getSocketData());
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
        std::string upgradeHeaderBuffer = std::string("GET /") + getData()->path + " HTTP/1.1\r\n"
                                                                                   "Upgrade: websocket\r\n"
                                                                                   "Connection: Upgrade\r\n"
                                                                                   "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"
                                                                                   "Host: " + getData()->host + "\r\n"
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
void HTTPSocket<isServer>::onEnd(uS::Socket s) {
    s.cancelTimeout();

    Data *httpSocketData = (Data *) s.getSocketData();
    s.close();

    if (!isServer) {
        ((Group<CLIENT> *) httpSocketData->nodeData)->errorHandler(httpSocketData->httpUser);
    }

    delete httpSocketData;
}

template struct HTTPSocket<SERVER>;
template struct HTTPSocket<CLIENT>;

}
