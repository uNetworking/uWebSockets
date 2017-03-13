#include "Hub.h"

namespace uWS {

template <const bool isServer>
bool WebSocketProtocol<isServer>::setCompressed(void *user) {  
    WebSocket<isServer> *webSocket = (WebSocket<isServer> *) user;

    if (webSocket->compressionStatus == WebSocket<isServer>::CompressionStatus::ENABLED) {
        webSocket->compressionStatus = WebSocket<isServer>::CompressionStatus::COMPRESSED_FRAME;
        return true;
    } else {
        return false;
    }
}

template <const bool isServer>
bool WebSocketProtocol<isServer>::refusePayloadLength(void *user, int length) {
    return length > 16777216;
}

template <const bool isServer>
void WebSocketProtocol<isServer>::forceClose(void *user) {
    WebSocket<isServer> *webSocket = (WebSocket<isServer> *) user;
    webSocket->terminate();
}

template <const bool isServer>
bool WebSocketProtocol<isServer>::handleFragment(char *data, size_t length, unsigned int remainingBytes, int opCode, bool fin, void *user) {
    WebSocket<isServer> *webSocket = (WebSocket<isServer> *) user;

    if (opCode < 3) {
        if (!remainingBytes && fin && !webSocket->fragmentBuffer.length()) {
            if (webSocket->compressionStatus == WebSocket<isServer>::CompressionStatus::COMPRESSED_FRAME) {
                webSocket->compressionStatus = WebSocket<isServer>::CompressionStatus::ENABLED;
                Hub *hub = ((Group<isServer> *) webSocket->nodeData)->hub;
                data = hub->inflate(data, length);
                if (!data) {
                    forceClose(user);
                    return true;
                }
            }

            if (opCode == 1 && !isValidUtf8((unsigned char *) data, length)) {
                forceClose(user);
                return true;
            }

            ((Group<isServer> *) webSocket->nodeData)->messageHandler(webSocket, data, length, (OpCode) opCode);
            if (webSocket->isClosed() || webSocket->isShuttingDown()) {
                return true;
            }
        } else {
            webSocket->fragmentBuffer.append(data, length);
            if (!remainingBytes && fin) {
                length = webSocket->fragmentBuffer.length();
                if (webSocket->compressionStatus == WebSocket<isServer>::CompressionStatus::COMPRESSED_FRAME) {
                    webSocket->compressionStatus = WebSocket<isServer>::CompressionStatus::ENABLED;
                    Hub *hub = ((Group<isServer> *) webSocket->nodeData)->hub;
                    webSocket->fragmentBuffer.append("....");
                    data = hub->inflate((char *) webSocket->fragmentBuffer.data(), length);
                    if (!data) {
                        forceClose(user);
                        return true;
                    }
                } else {
                    data = (char *) webSocket->fragmentBuffer.data();
                }

                if (opCode == 1 && !isValidUtf8((unsigned char *) data, length)) {
                    forceClose(user);
                    return true;
                }

                ((Group<isServer> *) webSocket->nodeData)->messageHandler(webSocket, data, length, (OpCode) opCode);
                if (webSocket->isClosed() || webSocket->isShuttingDown()) {
                    return true;
                }
                webSocket->fragmentBuffer.clear();
            }
        }
    } else {
        // todo: we don't need to buffer up in most cases!
        webSocket->controlBuffer.append(data, length);
        if (!remainingBytes && fin) {
            if (opCode == CLOSE) {
                CloseFrame closeFrame = parseClosePayload((char *) webSocket->controlBuffer.data(), webSocket->controlBuffer.length());
                webSocket->close(closeFrame.code, closeFrame.message, closeFrame.length);
                return true;
            } else {
                if (opCode == PING) {
                    webSocket->send(webSocket->controlBuffer.data(), webSocket->controlBuffer.length(), (OpCode) OpCode::PONG);
                    ((Group<isServer> *) webSocket->nodeData)->pingHandler(webSocket, (char *) webSocket->controlBuffer.data(), webSocket->controlBuffer.length());
                    if (webSocket->isClosed() || webSocket->isShuttingDown()) {
                        return true;
                    }
                } else if (opCode == PONG) {
                    ((Group<isServer> *) webSocket->nodeData)->pongHandler(webSocket, (char *) webSocket->controlBuffer.data(), webSocket->controlBuffer.length());
                    if (webSocket->isClosed() || webSocket->isShuttingDown()) {
                        return true;
                    }
                }
            }
            webSocket->controlBuffer.clear();
        }
    }

    return false;
}

template class WebSocketProtocol<SERVER>;
template class WebSocketProtocol<CLIENT>;

}
