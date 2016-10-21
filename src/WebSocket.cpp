#include "WebSocket.h"
#include "Group.h"

namespace uWS {

template <bool isServer>
void WebSocket<isServer>::send(const char *message, size_t length, OpCode opCode, void(*callback)(void *webSocket, void *data, bool cancelled), void *callbackData) {
    const int HEADER_LENGTH = WebSocketProtocol<!isServer>::LONG_MESSAGE_HEADER;

    if (hasEmptyQueue()) {
        if (length + sizeof(uS::SocketData::Queue::Message) + HEADER_LENGTH <= uS::NodeData::preAllocMaxSize) {
            int memoryLength = length + sizeof(uS::SocketData::Queue::Message) + HEADER_LENGTH;
            int memoryIndex = getSocketData()->nodeData->getMemoryBlockIndex(memoryLength);

            uS::SocketData::Queue::Message *messagePtr = (uS::SocketData::Queue::Message *) getSocketData()->nodeData->getSmallMemoryBlock(memoryIndex);
            messagePtr->data = ((char *) messagePtr) + sizeof(uS::SocketData::Queue::Message);
            messagePtr->length = WebSocketProtocol<isServer>::formatMessage((char *) messagePtr->data, message, length, opCode, length, false);

            bool wasTransferred;
            if (write(messagePtr, wasTransferred)) {
                if (!wasTransferred) {
                    getSocketData()->nodeData->freeSmallMemoryBlock((char *) messagePtr, memoryIndex);
                    if (callback) {
                        callback(*this, callbackData, false);
                    }
                } else {
                    messagePtr->callback = callback;
                    messagePtr->callbackData = callbackData;
                }
            } else {
                if (callback) {
                    callback(*this, callbackData, true);
                }
                onEnd(*this);
            }
        } else {
            uS::SocketData::Queue::Message *messagePtr = allocMessage(length + HEADER_LENGTH);
            messagePtr->length = WebSocketProtocol<isServer>::formatMessage((char *) messagePtr->data, message, length, opCode, length, false);
            bool wasTransferred;
            if (write(messagePtr, wasTransferred)) {
                if (!wasTransferred) {
                    freeMessage(messagePtr);
                    if (callback) {
                        callback(*this, callbackData, false);
                    }
                } else {
                    messagePtr->callback = callback;
                    messagePtr->callbackData = callbackData;
                }
            } else {
                if (callback) {
                    callback(*this, callbackData, true);
                }
                onEnd(*this);
            }
        }
    } else {
        uS::SocketData::Queue::Message *messagePtr = allocMessage(length + HEADER_LENGTH);
        messagePtr->length = WebSocketProtocol<isServer>::formatMessage((char *) messagePtr->data, message, length, opCode, length, false);
        messagePtr->callback = callback;
        messagePtr->callbackData = callbackData;
        enqueue(messagePtr);
    }
}

template <bool isServer>
typename WebSocket<isServer>::PreparedMessage *WebSocket<isServer>::prepareMessage(char *data, size_t length, OpCode opCode, bool compressed)
{
    PreparedMessage *preparedMessage = new PreparedMessage;
    preparedMessage->buffer = new char[length + 10];
    preparedMessage->length = WebSocketProtocol<isServer>::formatMessage(preparedMessage->buffer, data, length, opCode, length, compressed);
    preparedMessage->references = 1;
    return preparedMessage;
}

template <bool isServer>
void WebSocket<isServer>::sendPrepared(typename WebSocket<isServer>::PreparedMessage *preparedMessage)
{
    preparedMessage->references++;
    void (*callback)(void *webSocket, void *userData, bool cancelled) = [](void *webSocket, void *userData, bool cancelled) {
        PreparedMessage *preparedMessage = (PreparedMessage *) userData;
        if (!--preparedMessage->references) {
            delete [] preparedMessage->buffer;
            delete preparedMessage;
        }
    };

    // candidate for fixed size pool allocator
    int memoryLength = sizeof(uS::SocketData::Queue::Message);
    int memoryIndex = getSocketData()->nodeData->getMemoryBlockIndex(memoryLength);

    uS::SocketData::Queue::Message *messagePtr = (uS::SocketData::Queue::Message *) getSocketData()->nodeData->getSmallMemoryBlock(memoryIndex);
    messagePtr->data = preparedMessage->buffer;
    messagePtr->length = preparedMessage->length;

    bool wasTransferred;
    if (write(messagePtr, wasTransferred)) {
        if (!wasTransferred) {
            getSocketData()->nodeData->freeSmallMemoryBlock((char *) messagePtr, memoryIndex);
            if (callback) {
                callback(*this, preparedMessage, false);
            }
        } else {
            messagePtr->callback = callback;
            messagePtr->callbackData = preparedMessage;
        }
    } else {
        if (callback) {
            callback(*this, preparedMessage, true);
        }
        onEnd(*this);
    }
}

template <bool isServer>
void WebSocket<isServer>::finalizeMessage(typename WebSocket<isServer>::PreparedMessage *preparedMessage)
{
    if (!--preparedMessage->references) {
        delete [] preparedMessage->buffer;
        delete preparedMessage;
    }
}

template <bool isServer>
void WebSocket<isServer>::onData(uS::Socket s, char *data, int length) {
    Data *webSocketData = (Data *) s.getSocketData();
    webSocketData->hasOutstandingPong = false;
    if (!s.isShuttingDown()) {
        s.cork(true);
        ((WebSocketProtocol<isServer> *) webSocketData)->consume(data, length, s);
        if (!s.isClosed()) {
            s.cork(false);
        }
    }
}

template <bool isServer>
void WebSocket<isServer>::terminate() {
    WebSocket<isServer>::onEnd(*this);
}

template <bool isServer>
void WebSocket<isServer>::close(int code, char *message, size_t length) {
    static const int MAX_CLOSE_PAYLOAD = 123;
    length = std::min<size_t>(MAX_CLOSE_PAYLOAD, length);
    ((Group<isServer> *) getSocketData()->nodeData)->removeWebSocket(*this);
    ((Group<isServer> *) getSocketData()->nodeData)->disconnectionHandler(*this, code, message, length);
    getSocketData()->shuttingDown = true;
    startTimeout<WebSocket<isServer>::onEnd>();

    char closePayload[MAX_CLOSE_PAYLOAD + 2];
    int closePayloadLength = WebSocketProtocol<isServer>::formatClosePayload(closePayload, code, message, length);
    send(closePayload, closePayloadLength, OpCode::CLOSE, [](void *p, void *data, bool cancelled) {
        if (!cancelled) {
            Socket((uv_poll_t *) p).shutdown();
        }
    });
}

template <bool isServer>
void WebSocket<isServer>::onEnd(uS::Socket s) {
    if (!s.isShuttingDown()) {
        ((Group<isServer> *) s.getSocketData()->nodeData)->removeWebSocket(s);
        ((Group<isServer> *) s.getSocketData()->nodeData)->disconnectionHandler(WebSocket<isServer>(s), 1006, nullptr, 0);
    } else {
        s.cancelTimeout();
    }

    Data *webSocketData = (Data *) s.getSocketData();

    s.close();

    while (!webSocketData->messageQueue.empty()) {
        uS::SocketData::Queue::Message *message = webSocketData->messageQueue.front();
        if (message->callback) {
            message->callback(nullptr, message->callbackData, true);
        }
        webSocketData->messageQueue.pop();
    }

    delete webSocketData;
}

template struct WebSocket<SERVER>;
template struct WebSocket<CLIENT>;

}
