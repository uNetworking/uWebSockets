#include "HTTPSocket.h"
#include "Group.h"
#include "Extensions.h"

namespace uWS {

struct HTTPParser {
    char *cursor;
    std::pair<char *, size_t> key, value;
    HTTPParser(char *cursor) : cursor(cursor)
    {
        size_t length;
        for (; isspace(*cursor); cursor++);
        for (length = 0; !isspace(cursor[length]) && cursor[length] != '\r'; length++);
        key = {cursor, length};
        cursor += length + 1;
        for (length = 0; !isspace(cursor[length]) && cursor[length] != '\r'; length++);
        value = {cursor, length};
    }
    HTTPParser &operator++(int)
    {
        size_t length = 0;
        for (; !(cursor[0] == '\r' && cursor[1] == '\n'); cursor++);
        cursor += 2;
        if (cursor[0] == '\r' && cursor[1] == '\n') {
            key = value = {0, 0};
        } else {
            for (; cursor[length] != ':' && cursor[length] != '\r'; length++);
            key = {cursor, length};
            if (cursor[length] != '\r') {
                cursor += length;
                length = 0;
                while (isspace(*(++cursor)));
                for (; cursor[length] != '\r'; length++);
                value = {cursor, length};
            } else {
                value = {0, 0};
            }
        }
        return *this;
    }
};

static void base64(unsigned char *src, char *dst)
{
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

    // 5k = force close!
    if (httpData->httpBuffer.length() + length > 1024 * 5) {
        httpSocket.onEnd(s);
        return;
    }

    httpData->httpBuffer.append(data, length);

    size_t endOfHTTPBuffer = httpData->httpBuffer.find("\r\n\r\n");
    if (endOfHTTPBuffer != std::string::npos) {

        int enable = 1;
        setsockopt(httpSocket.getFd(), IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable));

        if (isServer) {

            HTTPParser httpParser = (char *) httpData->httpBuffer.data();
            std::pair<char *, size_t> secKey = {}, extensions = {}, subprotocol = {}, path = httpParser.value;
            for (httpParser++; httpParser.key.second; httpParser++) {
                if (httpParser.key.second == 17 || httpParser.key.second == 24 || httpParser.key.second == 22) {
                    // lowercase the key
                    for (size_t i = 0; i < httpParser.key.second; i++) {
                        httpParser.key.first[i] = tolower(httpParser.key.first[i]);
                    }
                    if (!strncmp(httpParser.key.first, "sec-websocket-key", httpParser.key.second)) {
                        secKey = httpParser.value;
                    } else if (!strncmp(httpParser.key.first, "sec-websocket-extensions", httpParser.key.second)) {
                        extensions = httpParser.value;
                    } else if (!strncmp(httpParser.key.first, "sec-websocket-protocol", httpParser.key.second)) {
                        subprotocol = httpParser.value;
                    }
                }
            }

            if (secKey.first && secKey.second == 24) {

                // this needs to be part of upgrade itself, and shared with Hub::upgrade!
                bool perMessageDeflate = false;
                std::string extensionsResponse;
                if (extensions.first) {
                    Group<isServer> *group = (Group<isServer> *) s.getNodeData(s.getSocketData());
                    ExtensionsNegotiator<uWS::SERVER> extensionsNegotiator(group->extensionOptions);
                    extensionsNegotiator.readOffer(std::string(extensions.first, extensions.second));
                    extensionsResponse = extensionsNegotiator.generateOffer();
                    if (extensionsNegotiator.getNegotiatedOptions() & PERMESSAGE_DEFLATE) {
                        perMessageDeflate = true;
                    }
                }

                if (httpSocket.upgrade(secKey.first, extensionsResponse.data(), extensionsResponse.length(), subprotocol.first, subprotocol.second)) {
                    s.cancelTimeout();
                    s.enterState<WebSocket<SERVER>>(new WebSocket<SERVER>::Data(perMessageDeflate, httpData));

                    ((Group<SERVER> *) s.getSocketData()->nodeData)->addWebSocket(s);
                    //s.cork(true);
                    ((Group<SERVER> *) s.getSocketData()->nodeData)->connectionHandler(WebSocket<SERVER>(s), {path.first, subprotocol.first,
                                                                                                              path.second, subprotocol.second});
                    //s.cork(false);
                    delete httpData;
                }
            } else {
                httpSocket.onEnd(s);
            }
        } else {
            httpData->httpBuffer.resize(httpData->httpBuffer.length() + WebSocketProtocol<uWS::CLIENT>::CONSUME_POST_PADDING);

            bool isUpgrade = false;
            HTTPParser httpParser = (char *) httpData->httpBuffer.data();
            //std::pair<char *, size_t> secKey = {}, extensions = {};
            for (httpParser++; httpParser.key.second; httpParser++) {
                if (httpParser.key.second == 7) {
                    // lowercase the key
                    for (size_t i = 0; i < httpParser.key.second; i++) {
                        httpParser.key.first[i] = tolower(httpParser.key.first[i]);
                    }
                    // lowercase the value
                    for (size_t i = 0; i < httpParser.value.second; i++) {
                        httpParser.value.first[i] = tolower(httpParser.value.first[i]);
                    }
                    if (!strncmp(httpParser.key.first, "upgrade", httpParser.key.second)) {
                        if (!strncmp(httpParser.value.first, "websocket", 9)) {
                            isUpgrade = true;
                        }
                        break;
                    }
                }
            }

            if (isUpgrade) {
                s.enterState<WebSocket<CLIENT>>(new WebSocket<CLIENT>::Data(false, httpData));

                //s.cork(true);
                httpSocket.cancelTimeout();
                httpSocket.setUserData(httpData->httpUser);
                ((Group<CLIENT> *) s.getSocketData()->nodeData)->addWebSocket(s);
                ((Group<CLIENT> *) s.getSocketData()->nodeData)->connectionHandler(WebSocket<CLIENT>(s), {nullptr, nullptr, 0, 0});
                //s.cork(false);

                if (!(s.isClosed() || s.isShuttingDown())) {
                    WebSocketProtocol<CLIENT> *kws = (WebSocketProtocol<CLIENT> *) ((WebSocket<CLIENT>::Data *) s.getSocketData());
                    kws->consume((char *) httpData->httpBuffer.data() + endOfHTTPBuffer + 4, httpData->httpBuffer.length() - endOfHTTPBuffer - 4 - WebSocketProtocol<uWS::CLIENT>::CONSUME_POST_PADDING, s);
                }

                delete httpData;
            } else {
                httpSocket.onEnd(s);
            }
        }
    }
}

template <bool isServer>
bool HTTPSocket<isServer>::upgrade(const char *secKey, const char *extensions, size_t extensionsLength, const char *subprotocol, size_t subprotocolLength) {
    if (isServer) {
        unsigned char shaInput[] = "XXXXXXXXXXXXXXXXXXXXXXXX258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        memcpy(shaInput, secKey, 24);
        unsigned char shaDigest[SHA_DIGEST_LENGTH];
        SHA1(shaInput, sizeof(shaInput) - 1, shaDigest);

        char upgradeBuffer[1024];
        memcpy(upgradeBuffer, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ", 97);
        base64(shaDigest, upgradeBuffer + 97);
        memcpy(upgradeBuffer + 125, "\r\n", 2);
        size_t upgradeResponseLength = 127;
        if (extensionsLength) {
            memcpy(upgradeBuffer + upgradeResponseLength, "Sec-WebSocket-Extensions: ", 26);
            memcpy(upgradeBuffer + upgradeResponseLength + 26, extensions, extensionsLength);
            memcpy(upgradeBuffer + upgradeResponseLength + 26 + extensionsLength, "\r\n", 2);
            upgradeResponseLength += 26 + extensionsLength + 2;
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
