#ifndef SOCKET_UWS_H
#define SOCKET_UWS_H

#include "Networking.h"

#include <iostream>

namespace uS {

struct WIN32_EXPORT Socket : SocketData {

    Socket(NodeData *nodeData, Loop *loop, uv_os_sock_t fd) : SocketData(nodeData, loop, fd) {

    }

    void transfer(NodeData *nodeData, void (*cb)(Poll *)) {
        if (threadSafeTransfer(this->nodeData->loop, nodeData->loop, getPoll())) {
            this->nodeData = nodeData;
            cb(this);
        } else {
//            // todo: libuv is not thread safe
//            nodeData->asyncMutex->lock();
//            //nodeData->transferQueue.push_back({new Poll, getFd(), socketData, getPollCallback(), cb});
//            nodeData->asyncMutex->unlock();

//            if (nodeData->tid != nodeData->tid) {
//                nodeData->async->send();
//            } else {
//                NodeData::asyncCallback(nodeData->async);
//            }

//            stop(nodeData->loop);
//            SocketData::close(nodeData->loop);
        }
    }

    void changePoll(SocketData *socketData) {
        if (!threadSafeChange(nodeData->loop, this, socketData->getPoll())) {

//            if (socketData->nodeData->tid != pthread_self()) {
//                socketData->nodeData->asyncMutex->lock();
//                socketData->nodeData->changePollQueue.push_back(socketData);
//                socketData->nodeData->asyncMutex->unlock();
//                socketData->nodeData->async->send();
//            } else {
//                change(socketData->nodeData->loop, socketData, socketData->getPoll());
//            }
        }
    }

    // this is not used?
//    static Poll *init(NodeData *nodeData, uv_os_sock_t fd, SSL *ssl) {
//        if (ssl) {
//            SSL_set_fd(ssl, fd);
//            SSL_set_mode(ssl, SSL_MODE_RELEASE_BUFFERS);
//        }

//        SocketData *socketData = new SocketData(nodeData, nodeData->loop, fd);
//        socketData->ssl = ssl;
//        socketData->state.poll = UV_READABLE;
//        return socketData;
//    }

    void *getUserData() {
        return user;
    }

    void setUserData(void *user) {
        this->user = user;
    }

    bool isShuttingDown() {
        return state.shuttingDown;
    }

    struct Address {
        unsigned int port;
        const char *address;
        const char *family;
    };

    Address getAddress();

    void cork(int enable) {
#if defined(TCP_CORK)
        // Linux & SmartOS have proper TCP_CORK
        setsockopt(getFd(), IPPROTO_TCP, TCP_CORK, &enable, sizeof(int));
#elif defined(TCP_NOPUSH)
        // Mac OS X & FreeBSD have TCP_NOPUSH
        setsockopt(getFd(), IPPROTO_TCP, TCP_NOPUSH, &enable, sizeof(int));
        if (!enable) {
            // Tested on OS X, FreeBSD situation is unclear
            ::send(getFd(), "", 0, MSG_NOSIGNAL);
        }
#endif
    }

    void setNoDelay(int enable) {
        setsockopt(getFd(), IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
    }

    void shutdown() {
        if (ssl) {
            //todo: poll in/out - have the io_cb recall shutdown if failed
            SSL_shutdown(ssl);
        } else {
            ::shutdown(getFd(), SHUT_WR);
        }
    }

    // clears user data!
    template <void onTimeout(Socket *)>
    void startTimeout(int timeoutMs = 15000) {
        Timer *timer = new Timer(nodeData->loop);
        timer->setData(this);
        timer->start([](Timer *timer) {
            Socket *s = (Socket *) timer->getData();
            s->cancelTimeout();
            onTimeout(s);
        }, timeoutMs, 0);

        user = timer;
    }

    void cancelTimeout() {
        Timer *timer = (Timer *) getUserData();
        if (timer) {
            timer->stop();
            timer->close();
            user = nullptr;
        }
    }

    template <class STATE>
    static void sslIoHandler(Poll *p, int status, int events) {
        Socket *socket = (Socket *) p;

        if (status < 0) {
            STATE::onEnd((Socket *) p);
            return;
        }

        if (!socket->messageQueue.empty() && ((events & UV_WRITABLE) || SSL_want(socket->ssl) == SSL_READING)) {
            socket->cork(true);
            while (true) {
                SocketData::Queue::Message *messagePtr = socket->messageQueue.front();
                int sent = SSL_write(socket->ssl, messagePtr->data, messagePtr->length);
                if (sent == (ssize_t) messagePtr->length) {
                    if (messagePtr->callback) {
                        messagePtr->callback(p, messagePtr->callbackData, false, messagePtr->reserved);
                    }
                    socket->messageQueue.pop();
                    if (socket->messageQueue.empty()) {
                        if ((socket->state.poll & UV_WRITABLE) && SSL_want(socket->ssl) != SSL_WRITING) {
                            p->change(socket->nodeData->loop, socket, socket->setPoll(UV_READABLE));
                        }
                        break;
                    }
                } else if (sent <= 0) {
                    switch (SSL_get_error(socket->ssl, sent)) {
                    case SSL_ERROR_WANT_READ:
                        break;
                    case SSL_ERROR_WANT_WRITE:
                        if ((socket->getPoll() & UV_WRITABLE) == 0) {
                            p->change(socket->nodeData->loop, socket, socket->setPoll(socket->getPoll() | UV_WRITABLE));
                        }
                        break;
                    default:
                        STATE::onEnd((Socket *) p);
                        return;
                    }
                    break;
                }
            }
            socket->cork(false);
        }

        if (events & UV_READABLE) {
            do {
                int length = SSL_read(socket->ssl, socket->nodeData->recvBuffer, socket->nodeData->recvLength);
                if (length <= 0) {
                    switch (SSL_get_error(socket->ssl, length)) {
                    case SSL_ERROR_WANT_READ:
                        break;
                    case SSL_ERROR_WANT_WRITE:
                        if ((socket->getPoll() & UV_WRITABLE) == 0) {
                            p->change(socket->nodeData->loop, socket, socket->setPoll(socket->getPoll() | UV_WRITABLE));
                        }
                        break;
                    default:
                        STATE::onEnd((Socket *) p);
                        return;
                    }
                    break;
                } else {
                    // Warning: onData can delete the socket! Happens when HttpSocket upgrades
                    socket = STATE::onData((Socket *) p, socket->nodeData->recvBuffer, length);
                    if (socket->isClosed() || socket->isShuttingDown()) {
                        return;
                    }
                }
            } while (SSL_pending(socket->ssl));
        }
    }

    template <class STATE>
    static void ioHandler(Poll *p, int status, int events) {
        SocketData *socketData = (SocketData *) p;
        NodeData *nodeData = socketData->nodeData;
        Socket *socket = (Socket *) p;

        if (status < 0) {
            STATE::onEnd((Socket *) p);
            return;
        }

        if (events & UV_WRITABLE) {
            if (!socketData->messageQueue.empty() && (events & UV_WRITABLE)) {
                socket->cork(true);
                while (true) {
                    SocketData::Queue::Message *messagePtr = socketData->messageQueue.front();
                    ssize_t sent = ::send(socket->getFd(), messagePtr->data, messagePtr->length, MSG_NOSIGNAL);
                    if (sent == (ssize_t) messagePtr->length) {
                        if (messagePtr->callback) {
                            messagePtr->callback(p, messagePtr->callbackData, false, messagePtr->reserved);
                        }
                        socketData->messageQueue.pop();
                        if (socketData->messageQueue.empty()) {
                            // todo, remove bit, don't set directly
                            p->change(socketData->nodeData->loop, socketData, socketData->setPoll(UV_READABLE));
                            break;
                        }
                    } else if (sent == SOCKET_ERROR) {
                        if (errno != EWOULDBLOCK) {
                            STATE::onEnd((Socket *) p);
                            return;
                        }
                        break;
                    } else {
                        messagePtr->length -= sent;
                        messagePtr->data += sent;
                        break;
                    }
                }
                socket->cork(false);
            }
        }

        if (events & UV_READABLE) {
            int length = recv(socket->getFd(), nodeData->recvBuffer, nodeData->recvLength, 0);
            if (length > 0) {
                STATE::onData((Socket *) p, nodeData->recvBuffer, length);
            } else if (length <= 0 || (length == SOCKET_ERROR && errno != EWOULDBLOCK)) {
                STATE::onEnd((Socket *) p);
            }
        }

    }

    template<class STATE>
    void setState() {
        if (ssl) {
            setCb(sslIoHandler<STATE>);
        } else {
            setCb(ioHandler<STATE>);
        }
    }

    template <class T>
    void closeSocket() {
        uv_os_sock_t fd = getFd();
        stop(nodeData->loop);
        ::close(fd);

        if (ssl) {
            SSL_free(ssl);
        }

        Poll::close(nodeData->loop, [](Poll *p) {
            delete (T *) p;
        });
    }

    bool hasEmptyQueue() {
        return messageQueue.empty();
    }

    void enqueue(SocketData::Queue::Message *message) {
        messageQueue.push(message);
    }

    SocketData::Queue::Message *allocMessage(size_t length, const char *data = 0) {
        SocketData::Queue::Message *messagePtr = (SocketData::Queue::Message *) new char[sizeof(SocketData::Queue::Message) + length];
        messagePtr->length = length;
        messagePtr->data = ((char *) messagePtr) + sizeof(SocketData::Queue::Message);
        messagePtr->nextMessage = nullptr;

        if (data) {
            memcpy((char *) messagePtr->data, data, messagePtr->length);
        }

        return messagePtr;
    }

    void freeMessage(SocketData::Queue::Message *message) {
        delete [] (char *) message;
    }

    bool write(SocketData::Queue::Message *message, bool &wasTransferred) {
        ssize_t sent = 0;
        if (messageQueue.empty()) {

            if (ssl) {
                sent = SSL_write(ssl, message->data, message->length);
                if (sent == (ssize_t) message->length) {
                    wasTransferred = false;
                    return true;
                } else if (sent < 0) {
                    switch (SSL_get_error(ssl, sent)) {
                    case SSL_ERROR_WANT_READ:
                        break;
                    case SSL_ERROR_WANT_WRITE:
                        if ((getPoll() & UV_WRITABLE) == 0) {
                            setPoll(getPoll() | UV_WRITABLE);
                            changePoll(this);
                        }
                        break;
                    default:
                        return false;
                    }
                }
            } else {
                sent = ::send(getFd(), message->data, message->length, MSG_NOSIGNAL);
                if (sent == (ssize_t) message->length) {
                    wasTransferred = false;
                    return true;
                } else if (sent == SOCKET_ERROR) {
                    if (errno != EWOULDBLOCK) {
                        return false;
                    }
                } else {
                    message->length -= sent;
                    message->data += sent;
                }

                if ((getPoll() & UV_WRITABLE) == 0) {
                    setPoll(getPoll() | UV_WRITABLE);
                    changePoll(this);
                }
            }
        }
        messageQueue.push(message);
        wasTransferred = true;
        return true;
    }

    template <class T, class D>
    void sendTransformed(const char *message, size_t length, void(*callback)(void *socket, void *data, bool cancelled, void *reserved), void *callbackData, D transformData) {
        size_t estimatedLength = T::estimate(message, length) + sizeof(uS::SocketData::Queue::Message);

        if (hasEmptyQueue()) {
            if (estimatedLength <= uS::NodeData::preAllocMaxSize) {
                int memoryLength = estimatedLength;
                int memoryIndex = nodeData->getMemoryBlockIndex(memoryLength);

                uS::SocketData::Queue::Message *messagePtr = (uS::SocketData::Queue::Message *) nodeData->getSmallMemoryBlock(memoryIndex);
                messagePtr->data = ((char *) messagePtr) + sizeof(uS::SocketData::Queue::Message);
                messagePtr->length = T::transform(message, (char *) messagePtr->data, length, transformData);

                bool wasTransferred;
                if (write(messagePtr, wasTransferred)) {
                    if (!wasTransferred) {
                        nodeData->freeSmallMemoryBlock((char *) messagePtr, memoryIndex);
                        if (callback) {
                            callback(this, callbackData, false, nullptr);
                        }
                    } else {
                        messagePtr->callback = callback;
                        messagePtr->callbackData = callbackData;
                    }
                } else {
                    nodeData->freeSmallMemoryBlock((char *) messagePtr, memoryIndex);
                    if (callback) {
                        callback(this, callbackData, true, nullptr);
                    }
                }
            } else {
                uS::SocketData::Queue::Message *messagePtr = allocMessage(estimatedLength - sizeof(uS::SocketData::Queue::Message));
                messagePtr->length = T::transform(message, (char *) messagePtr->data, length, transformData);

                bool wasTransferred;
                if (write(messagePtr, wasTransferred)) {
                    if (!wasTransferred) {
                        freeMessage(messagePtr);
                        if (callback) {
                            callback(this, callbackData, false, nullptr);
                        }
                    } else {
                        messagePtr->callback = callback;
                        messagePtr->callbackData = callbackData;
                    }
                } else {
                    freeMessage(messagePtr);
                    if (callback) {
                        callback(this, callbackData, true, nullptr);
                    }
                }
            }
        } else {
            uS::SocketData::Queue::Message *messagePtr = allocMessage(estimatedLength - sizeof(uS::SocketData::Queue::Message));
            messagePtr->length = T::transform(message, (char *) messagePtr->data, length, transformData);
            messagePtr->callback = callback;
            messagePtr->callbackData = callbackData;
            enqueue(messagePtr);
        }
    }
};

}

#endif // SOCKET_UWS_H
