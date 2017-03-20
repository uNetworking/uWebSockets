#include "WebSocket.h"
#include "Group.h"

namespace uWS {

template <bool isServer>
void WebSocket<isServer>::send(const char *message, size_t length, OpCode opCode, void(*callback)(WebSocket<isServer> *webSocket, void *data, bool cancelled, void *reserved), void *callbackData) {
    const int HEADER_LENGTH = WebSocketProtocol<!isServer>::LONG_MESSAGE_HEADER;

    struct TransformData {
        OpCode opCode;
    } transformData = {opCode};

    struct WebSocketTransformer {
        static size_t estimate(const char *data, size_t length) {
            return length + HEADER_LENGTH;
        }

        static size_t transform(const char *src, char *dst, size_t length, TransformData transformData) {
            return WebSocketProtocol<isServer>::formatMessage(dst, src, length, transformData.opCode, length, false);
        }
    };

    sendTransformed<WebSocketTransformer>((char *) message, length, (void(*)(void *, void *, bool, void *)) callback, callbackData, transformData);
}

template <bool isServer>
typename WebSocket<isServer>::PreparedMessage *WebSocket<isServer>::prepareMessage(char *data, size_t length, OpCode opCode, bool compressed, void(*callback)(WebSocket<isServer> *webSocket, void *data, bool cancelled, void *reserved)) {
    PreparedMessage *preparedMessage = new PreparedMessage;
    preparedMessage->buffer = new char[length + 10];
    preparedMessage->length = WebSocketProtocol<isServer>::formatMessage(preparedMessage->buffer, data, length, opCode, length, compressed);
    preparedMessage->references = 1;
    preparedMessage->callback = (void(*)(void *, void *, bool, void *)) callback;
    return preparedMessage;
}

template <bool isServer>
typename WebSocket<isServer>::PreparedMessage *WebSocket<isServer>::prepareMessageBatch(std::vector<std::string> &messages, std::vector<int> &excludedMessages, OpCode opCode, bool compressed, void (*callback)(WebSocket<isServer> *, void *, bool, void *))
{
    // should be sent in!
    size_t batchLength = 0;
    for (size_t i = 0; i < messages.size(); i++) {
        batchLength += messages[i].length();
    }

    PreparedMessage *preparedMessage = new PreparedMessage;
    preparedMessage->buffer = new char[batchLength + 10 * messages.size()];

    int offset = 0;
    for (size_t i = 0; i < messages.size(); i++) {
        offset += WebSocketProtocol<isServer>::formatMessage(preparedMessage->buffer + offset, messages[i].data(), messages[i].length(), opCode, messages[i].length(), compressed);
    }
    preparedMessage->length = offset;
    preparedMessage->references = 1;
    preparedMessage->callback = (void(*)(void *, void *, bool, void *)) callback;
    return preparedMessage;
}

// todo: see if this can be made a transformer instead
template <bool isServer>
void WebSocket<isServer>::sendPrepared(typename WebSocket<isServer>::PreparedMessage *preparedMessage, void *callbackData) {
    preparedMessage->references++;
    void (*callback)(void *webSocket, void *userData, bool cancelled, void *reserved) = [](void *webSocket, void *userData, bool cancelled, void *reserved) {
        PreparedMessage *preparedMessage = (PreparedMessage *) userData;
        bool lastReference = !--preparedMessage->references;

        if (preparedMessage->callback) {
            preparedMessage->callback(webSocket, reserved, cancelled, (void *) lastReference);
        }

        if (lastReference) {
            delete [] preparedMessage->buffer;
            delete preparedMessage;
        }
    };

    // candidate for fixed size pool allocator
    int memoryLength = sizeof(Queue::Message);
    int memoryIndex = nodeData->getMemoryBlockIndex(memoryLength);

    Queue::Message *messagePtr = (Queue::Message *) nodeData->getSmallMemoryBlock(memoryIndex);
    messagePtr->data = preparedMessage->buffer;
    messagePtr->length = preparedMessage->length;

    bool wasTransferred;
    if (write(messagePtr, wasTransferred)) {
        if (!wasTransferred) {
            nodeData->freeSmallMemoryBlock((char *) messagePtr, memoryIndex);
            if (callback) {
                callback(this, preparedMessage, false, callbackData);
            }
        } else {
            messagePtr->callback = callback;
            messagePtr->callbackData = preparedMessage;
            messagePtr->reserved = callbackData;
        }
    } else {
        nodeData->freeSmallMemoryBlock((char *) messagePtr, memoryIndex);
        if (callback) {
            callback(this, preparedMessage, true, callbackData);
        }
    }
}

template <bool isServer>
void WebSocket<isServer>::finalizeMessage(typename WebSocket<isServer>::PreparedMessage *preparedMessage) {
    if (!--preparedMessage->references) {
        delete [] preparedMessage->buffer;
        delete preparedMessage;
    }
}

template <bool isServer>
uS::Socket *WebSocket<isServer>::onData(uS::Socket *s, char *data, size_t length) {
    WebSocket<isServer> *webSocket = (WebSocket<isServer> *) s;

    webSocket->hasOutstandingPong = false;
    if (!webSocket->isShuttingDown()) {
        webSocket->cork(true);
        webSocket->consume(data, length, s);
        if (!webSocket->isClosed()) {
            webSocket->cork(false);
        }
    }

    return webSocket;
}

template <bool isServer>
void WebSocket<isServer>::terminate() {
    WebSocket<isServer>::onEnd(this);
}

template <bool isServer>
void WebSocket<isServer>::close(int code, const char *message, size_t length) {
    static const int MAX_CLOSE_PAYLOAD = 123;
    length = std::min<size_t>(MAX_CLOSE_PAYLOAD, length);
    getGroup<isServer>(this)->removeWebSocket(this);
    getGroup<isServer>(this)->disconnectionHandler(this, code, (char *) message, length);
    setShuttingDown(true);

    // todo: using the shared timer in the group, we can skip creating a new timer per socket
    // only this line and the one in Hub::connect uses the timeout feature
    startTimeout<WebSocket<isServer>::onEnd>();

    char closePayload[MAX_CLOSE_PAYLOAD + 2];
    int closePayloadLength = WebSocketProtocol<isServer>::formatClosePayload(closePayload, code, message, length);
    send(closePayload, closePayloadLength, OpCode::CLOSE, [](WebSocket<isServer> *p, void *data, bool cancelled, void *reserved) {
        if (!cancelled) {
            p->shutdown();
        }
    });
}

template <bool isServer>
void WebSocket<isServer>::onEnd(uS::Socket *s) {
    WebSocket<isServer> *webSocket = (WebSocket<isServer> *) s;

    if (!webSocket->isShuttingDown()) {
        getGroup<isServer>(webSocket)->removeWebSocket(webSocket);
        getGroup<isServer>(webSocket)->disconnectionHandler(webSocket, 1006, nullptr, 0);
    } else {
        webSocket->cancelTimeout();
    }

    webSocket->template closeSocket<WebSocket<isServer>>();

    while (!webSocket->messageQueue.empty()) {
        Queue::Message *message = webSocket->messageQueue.front();
        if (message->callback) {
            message->callback(nullptr, message->callbackData, true, nullptr);
        }
        webSocket->messageQueue.pop();
    }
}

template struct WebSocket<SERVER>;
template struct WebSocket<CLIENT>;

}
