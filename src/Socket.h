#ifndef SOCKET_UWS_H
#define SOCKET_UWS_H

#include "Networking.h"

namespace uS {

class WIN32_EXPORT Socket {
protected:
    uv_poll_t *p;

public:
    Socket(uv_poll_t *p) : p(p) {

    }

    uv_poll_cb getPollCallback() {
#ifdef USE_MICRO_UV
        return p->get_poll_cb();
#else
        return p->poll_cb;
#endif
    }

    void transfer(NodeData *nodeData, void (*cb)(uv_poll_t *)) {
        SocketData *socketData = getSocketData();

        nodeData->asyncMutex->lock();
        nodeData->transferQueue.push_back({new uv_poll_t, getFd(), socketData, getPollCallback(), cb});
        nodeData->asyncMutex->unlock();

        if (socketData->nodeData->tid != nodeData->tid) {
            uv_async_send(nodeData->async);
        } else {
            NodeData::asyncCallback(nodeData->async);
        }

        uv_poll_stop(p);
        uv_close(p, [](uv_handle_t *h) {
            delete (uv_poll_t *) h;
        });
    }

    static uv_poll_t *init(NodeData *nodeData, uv_os_sock_t fd, SSL *ssl) {
        if (ssl) {
            SSL_set_fd(ssl, fd);
            SSL_set_mode(ssl, SSL_MODE_RELEASE_BUFFERS);
        }

        SocketData *socketData = new SocketData(nodeData);
        socketData->ssl = ssl;
        socketData->poll = UV_READABLE;

        uv_poll_t *p = new uv_poll_t;
        uv_poll_init_socket(nodeData->loop, p, fd);
        p->data = socketData;
        return p;
    }

    uv_os_sock_t getFd() {
#ifdef _WIN32
        uv_os_sock_t fd;
        uv_fileno((uv_handle_t *) p, (uv_os_fd_t *) &fd);
        return fd;
#else
#ifdef USE_MICRO_UV
        return p->fd;
#else
        return p->io_watcher.fd;
#endif
#endif
    }

    SocketData *getSocketData() {
        return (SocketData *) p->data;
    }

    NodeData *getNodeData(SocketData *socketData) {
        return socketData->nodeData;
    }

    void *getUserData() {
        return getSocketData()->user;
    }

    void setUserData(void *user) {
        getSocketData()->user = user;
    }

    operator uv_poll_t *() const {
        return p;
    }

    bool isClosed() {
        return uv_is_closing(p);
    }

    bool isShuttingDown() {
        return getSocketData()->shuttingDown;
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
        SSL *ssl = getSocketData()->ssl;
        if (ssl) {
            //todo: poll in/out - have the io_cb recall shutdown if failed
            SSL_shutdown(ssl);
        } else {
            ::shutdown(getFd(), SHUT_WR);
        }
    }

    // clears user data!
    template <void onTimeout(Socket)>
    void startTimeout(int timeoutMs = 15000) {
        SocketData *socketData = getSocketData();
        NodeData *nodeData = getNodeData(socketData);

        uv_timer_t *timer = new uv_timer_t;
        timer->data = p;
        uv_timer_init(nodeData->loop, timer);
        uv_timer_start(timer, [](uv_timer_t *timer) {
            Socket s((uv_poll_t *) timer->data);
            s.cancelTimeout();
            onTimeout(s);
        }, timeoutMs, 0);

        socketData->user = timer;
    }

    void cancelTimeout() {
        uv_timer_t *timer = (uv_timer_t *) getUserData();
        if (timer) {
            uv_timer_stop(timer);
            uv_close(timer, [](uv_handle_t *handle) {
                delete (uv_timer_t *) handle;
            });
            getSocketData()->user = nullptr;
        }
    }

    template <class STATE>
    static void ssl_io_cb(uv_poll_t *p, int status, int events) {
        SocketData *socketData = Socket(p).getSocketData();
        NodeData *nodeData = socketData->nodeData;
        SSL *ssl = socketData->ssl;

        if (status < 0) {
            STATE::onEnd(p);
            return;
        }

        if (!socketData->messageQueue.empty() && ((events & UV_WRITABLE) || SSL_want(socketData->ssl) == SSL_READING)) {
            Socket(p).cork(true);
            while (true) {
                SocketData::Queue::Message *messagePtr = socketData->messageQueue.front();
                int sent = SSL_write(socketData->ssl, messagePtr->data, messagePtr->length);
                if (sent == (ssize_t) messagePtr->length) {
                    if (messagePtr->callback) {
                        messagePtr->callback(p, messagePtr->callbackData, false, messagePtr->reserved);
                    }
                    socketData->messageQueue.pop();
                    if (socketData->messageQueue.empty()) {
                        if ((socketData->poll & UV_WRITABLE) && SSL_want(socketData->ssl) != SSL_WRITING) {
                            // todo, remove bit, don't set directly
                            socketData->poll = UV_READABLE;
                            uv_poll_start(p, UV_READABLE, Socket(p).getPollCallback());
                        }
                        break;
                    }
                } else if (sent <= 0) {
                    switch (SSL_get_error(socketData->ssl, sent)) {
                    case SSL_ERROR_WANT_READ:
                        break;
                    case SSL_ERROR_WANT_WRITE:
                        if ((socketData->poll & UV_WRITABLE) == 0) {
                            socketData->poll |= UV_WRITABLE;
                            uv_poll_start(p, socketData->poll, Socket(p).getPollCallback());
                        }
                        break;
                    default:
                        STATE::onEnd(p);
                        return;
                    }
                    break;
                }
            }
            Socket(p).cork(false);
        }

        if (events & UV_READABLE) {
            do {
                int length = SSL_read(ssl, nodeData->recvBuffer, nodeData->recvLength);
                if (length <= 0) {
                    switch (SSL_get_error(ssl, length)) {
                    case SSL_ERROR_WANT_READ:
                        break;
                    case SSL_ERROR_WANT_WRITE:
                        if ((socketData->poll & UV_WRITABLE) == 0) {
                            socketData->poll |= UV_WRITABLE;
                            uv_poll_start(p, socketData->poll, Socket(p).getPollCallback());
                        }
                        break;
                    default:
                        STATE::onEnd(p);
                        return;
                    }
                    break;
                } else {
                    STATE::onData(p, nodeData->recvBuffer, length);
                    if (Socket(p).isClosed() || Socket(p).isShuttingDown()) {
                        return;
                    }
                }
            } while (SSL_pending(ssl));
        }
    }

    template <class STATE>
    static void io_cb(uv_poll_t *p, int status, int events) {
        SocketData *socketData = Socket(p).getSocketData();
        NodeData *nodeData = socketData->nodeData;

        if (status < 0) {
            STATE::onEnd(p);
            return;
        }

        if (events & UV_WRITABLE) {
            if (!socketData->messageQueue.empty() && (events & UV_WRITABLE)) {
                Socket(p).cork(true);
                while (true) {
                    SocketData::Queue::Message *messagePtr = socketData->messageQueue.front();
                    ssize_t sent = ::send(Socket(p).getFd(), messagePtr->data, messagePtr->length, MSG_NOSIGNAL);
                    if (sent == (ssize_t) messagePtr->length) {
                        if (messagePtr->callback) {
                            messagePtr->callback(p, messagePtr->callbackData, false, messagePtr->reserved);
                        }
                        socketData->messageQueue.pop();
                        if (socketData->messageQueue.empty()) {
                            // todo, remove bit, don't set directly
                            socketData->poll = UV_READABLE;
                            uv_poll_start(p, UV_READABLE, Socket(p).getPollCallback());
                            break;
                        }
                    } else if (sent == SOCKET_ERROR) {
                        if (errno != EWOULDBLOCK) {
                            STATE::onEnd(p);
                            return;
                        }
                        break;
                    } else {
                        messagePtr->length -= sent;
                        messagePtr->data += sent;
                        break;
                    }
                }
                Socket(p).cork(false);
            }
        }

        if (events & UV_READABLE) {
            int length = recv(Socket(p).getFd(), nodeData->recvBuffer, nodeData->recvLength, 0);
            if (length > 0) {
                STATE::onData(p, nodeData->recvBuffer, length);
            } else if (length <= 0 || (length == SOCKET_ERROR && errno != EWOULDBLOCK)) {
                STATE::onEnd(p);
            }
        }

    }

    template<class STATE>
    void enterState(void *socketData) {
        p->data = socketData;
        if (Socket(p).getSocketData()->ssl) {
            uv_poll_start(p, Socket(p).getSocketData()->poll, ssl_io_cb<STATE>);
        } else {
            uv_poll_start(p, Socket(p).getSocketData()->poll, io_cb<STATE>);
        }
        Socket(p).getSocketData()->poll = UV_READABLE;
    }

    /*void setPoll(int poll = UV_READABLE) {
        Socket(p).getSocketData()->poll = poll;
    }*/

    // does not change STATE, only poll for current state
    /*void addPollBit(int events) {
        uv_poll_start(p, p->flags, p->poll_cb);
    }

    void removePollBit(int events) {
        uv_poll_start(p, p->flags, p->poll_cb);
    }*/

    void close() {
        uv_os_sock_t fd = getFd();
        uv_poll_stop(p);
        ::close(fd);

        SSL *ssl = getSocketData()->ssl;
        if (ssl) {
            SSL_free(ssl);
        }

        uv_close(p, [](uv_handle_t *h) {
            delete (uv_poll_t *) h;
        });
    }

    bool hasEmptyQueue() {
        return getSocketData()->messageQueue.empty();
    }

    void enqueue(SocketData::Queue::Message *message) {
        getSocketData()->messageQueue.push(message);
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

    void changePoll(SocketData *socketData) {
        if (socketData->nodeData->tid != pthread_self()) {
            socketData->nodeData->asyncMutex->lock();
            socketData->nodeData->changePollQueue.push_back(p);
            socketData->nodeData->asyncMutex->unlock();
            uv_async_send(socketData->nodeData->async);
        } else {
            uv_poll_start(p, socketData->poll, getPollCallback());
        }
    }

    bool write(SocketData::Queue::Message *message, bool &wasTransferred) {
        ssize_t sent = 0;
        SocketData *socketData = getSocketData();
        if (socketData->messageQueue.empty()) {

            if (socketData->ssl) {
                sent = SSL_write(socketData->ssl, message->data, message->length);
                if (sent == (ssize_t) message->length) {
                    wasTransferred = false;
                    return true;
                } else if (sent < 0) {
                    switch (SSL_get_error(socketData->ssl, sent)) {
                    case SSL_ERROR_WANT_READ:
                        break;
                    case SSL_ERROR_WANT_WRITE:
                        if ((socketData->poll & UV_WRITABLE) == 0) {
                            socketData->poll |= UV_WRITABLE;
                            changePoll(socketData);
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

                if ((socketData->poll & UV_WRITABLE) == 0) {
                    socketData->poll |= UV_WRITABLE;
                    changePoll(socketData);
                }
            }
        }
        socketData->messageQueue.push(message);
        wasTransferred = true;
        return true;
    }

    template <class T, class D>
    void sendTransformed(const char *message, size_t length, void(*callback)(void *httpSocket, void *data, bool cancelled, void *reserved), void *callbackData, D transformData) {
        size_t estimatedLength = T::estimate(message, length) + sizeof(uS::SocketData::Queue::Message);

        if (hasEmptyQueue()) {
            if (estimatedLength <= uS::NodeData::preAllocMaxSize) {
                int memoryLength = estimatedLength;
                int memoryIndex = getSocketData()->nodeData->getMemoryBlockIndex(memoryLength);

                uS::SocketData::Queue::Message *messagePtr = (uS::SocketData::Queue::Message *) getSocketData()->nodeData->getSmallMemoryBlock(memoryIndex);
                messagePtr->data = ((char *) messagePtr) + sizeof(uS::SocketData::Queue::Message);
                messagePtr->length = T::transform(message, (char *) messagePtr->data, length, transformData);

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
                uS::SocketData::Queue::Message *messagePtr = allocMessage(estimatedLength - sizeof(uS::SocketData::Queue::Message));
                messagePtr->length = T::transform(message, (char *) messagePtr->data, length, transformData);

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
