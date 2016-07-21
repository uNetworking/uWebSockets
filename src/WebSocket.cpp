#include "Network.h"
#include "WebSocket.h"
#include "Server.h"
#include "Extensions.h"
#include "SocketData.h"
#include "Parser.h"

#include <iostream>
#include <algorithm>
#include <openssl/ssl.h>

namespace uWS {

inline size_t formatMessage(char *dst, char *src, size_t length, OpCode opCode, size_t reportedLength, bool compressed)
{
    size_t messageLength;
    if (reportedLength < 126) {
        messageLength = length + 2;
        memcpy(dst + 2, src, length);
        dst[1] = reportedLength;
    } else if (reportedLength <= UINT16_MAX) {
        messageLength = length + 4;
        memcpy(dst + 4, src, length);
        dst[1] = 126;
        *((uint16_t *) &dst[2]) = htons(reportedLength);
    } else {
        messageLength = length + 10;
        memcpy(dst + 10, src, length);
        dst[1] = 127;
        *((uint64_t *) &dst[2]) = htobe64(reportedLength);
    }

    int flags = 0;
    dst[0] = (flags & SND_NO_FIN ? 0 : 128) | (compressed ? SND_COMPRESSED : 0);
    if (!(flags & SND_CONTINUATION)) {
        dst[0] |= opCode;
    }
    return messageLength;
}

void WebSocket::send(char *message, size_t length, OpCode opCode, void (*callback)(WebSocket webSocket, void *data, bool cancelled), void *callbackData, size_t fakedLength)
{
    size_t reportedLength = length;
    if (fakedLength) {
        reportedLength = fakedLength;
    }

    if (length <= Server::SHORT_BUFFER_SIZE - 10) {
        SocketData *socketData = (SocketData *) p->data;
        char *sendBuffer = socketData->server->sendBuffer;
        write(sendBuffer, formatMessage(sendBuffer, message, length, opCode, reportedLength, false), false, callback, callbackData);
    } else {
        char *buffer = new char[sizeof(SocketData::Queue::Message) + length + 10] + sizeof(SocketData::Queue::Message);
        write(buffer, formatMessage(buffer, message, length, opCode, reportedLength, false), true, callback, callbackData);
    }
}

void WebSocket::ping(char *message, size_t length)
{
    send(message, length, OpCode::PING);
}

void WebSocket::sendFragment(char *data, size_t length, OpCode opCode, size_t remainingBytes)
{
    SocketData *socketData = (SocketData *) p->data;
    if (remainingBytes) {
        if (socketData->sendState == FRAGMENT_START) {
            send(data, length, opCode, nullptr, nullptr, length + remainingBytes);
            socketData->sendState = FRAGMENT_MID;
        } else {
            write(data, length, false);
        }
    } else {
        if (socketData->sendState == FRAGMENT_START) {
            send(data, length, opCode);
        } else {
            write(data, length, false);
            socketData->sendState = FRAGMENT_START;
        }
    }
}

WebSocket::PreparedMessage *WebSocket::prepareMessage(char *data, size_t length, OpCode opCode, bool compressed)
{
    PreparedMessage *preparedMessage = new PreparedMessage;
    preparedMessage->buffer = new char[sizeof(SocketData::Queue::Message) + length + 10] + sizeof(SocketData::Queue::Message);
    preparedMessage->length = formatMessage(preparedMessage->buffer, data, length, opCode, length, compressed);
    preparedMessage->references = 1;
    return preparedMessage;
}

void WebSocket::sendPrepared(WebSocket::PreparedMessage *preparedMessage)
{
    preparedMessage->references++;
    write(preparedMessage->buffer, preparedMessage->length, false, [](WebSocket webSocket, void *userData, bool cancelled) {
        PreparedMessage *preparedMessage = (PreparedMessage *) userData;
        if (!--preparedMessage->references) {
            delete [] (preparedMessage->buffer - sizeof(SocketData::Queue::Message));
            delete preparedMessage;
        }
    }, preparedMessage, true);
}

void WebSocket::finalizeMessage(WebSocket::PreparedMessage *preparedMessage)
{
    if (!--preparedMessage->references) {
        delete [] (preparedMessage->buffer - sizeof(SocketData::Queue::Message));
        delete preparedMessage;
    }
}

void WebSocket::handleFragment(const char *fragment, size_t length, OpCode opCode, bool fin, size_t remainingBytes, bool compressed)
{
    SocketData *socketData = (SocketData *) p->data;

    // Text or binary
    if (opCode < 3) {

        // permessage-deflate
        if (compressed) {
            socketData->pmd->setInput((char *) fragment, length);
            size_t bufferSpace;
            try {
                while (!(bufferSpace = socketData->pmd->inflate(socketData->server->inflateBuffer, Server::LARGE_BUFFER_SIZE))) {
                    socketData->buffer.append(socketData->server->inflateBuffer, Server::LARGE_BUFFER_SIZE);
                }

                if (!remainingBytes && fin) {
                    unsigned char tail[4] = {0, 0, 255, 255};
                    socketData->pmd->setInput((char *) tail, 4);
                    if (!socketData->pmd->inflate(socketData->server->inflateBuffer + Server::LARGE_BUFFER_SIZE - bufferSpace, bufferSpace)) {
                        socketData->buffer.append(socketData->server->inflateBuffer + Server::LARGE_BUFFER_SIZE - bufferSpace, bufferSpace);
                        while (!(bufferSpace = socketData->pmd->inflate(socketData->server->inflateBuffer, Server::LARGE_BUFFER_SIZE))) {
                            socketData->buffer.append(socketData->server->inflateBuffer, Server::LARGE_BUFFER_SIZE);
                        }
                    }
                }
            } catch (...) {
                close(true, 1006);
                return;
            }

            fragment = socketData->server->inflateBuffer;
            length = Server::LARGE_BUFFER_SIZE - bufferSpace;
        }

        if (!remainingBytes && fin && !socketData->buffer.length()) {
            if ((socketData->server->maxPayload && length > socketData->server->maxPayload) || (opCode == 1 && !isValidUtf8((unsigned char *) fragment, length))) {
                close(true, 1006);
                return;
            }

            socketData->server->messageCallback(p, (char *) fragment, length, opCode);
        } else {
            if (socketData->server->maxPayload && length + socketData->buffer.length() > socketData->server->maxPayload) {
                close(true, 1006);
                return;
            }

            socketData->buffer.append(fragment, length);
            if (!remainingBytes && fin) {

                // Chapter 6
                if (opCode == 1 && !isValidUtf8((unsigned char *) socketData->buffer.c_str(), socketData->buffer.length())) {
                    close(true, 1006);
                    return;
                }

                socketData->server->messageCallback(p, (char *) socketData->buffer.c_str(), socketData->buffer.length(), opCode);
                socketData->buffer.clear();
            }
        }
    } else {
        socketData->controlBuffer.append(fragment, length);
        if (!remainingBytes && fin) {
            if (opCode == CLOSE) {
                std::tuple<unsigned short, char *, size_t> closeFrame = Parser::parseCloseFrame(socketData->controlBuffer);
                close(false, std::get<0>(closeFrame), std::get<1>(closeFrame), std::get<2>(closeFrame));
                // leave the controlBuffer with the close frame intact
                return;
            } else {
                if (opCode == PING) {
                    send((char *) socketData->controlBuffer.c_str(), socketData->controlBuffer.length(), OpCode::PONG);
                    socketData->server->pingCallback(p, (char *) socketData->controlBuffer.c_str(), socketData->controlBuffer.length());
                } else if (opCode == PONG) {
                    socketData->server->pongCallback(p, (char *) socketData->controlBuffer.c_str(), socketData->controlBuffer.length());
                }
            }
            socketData->controlBuffer.clear();
        }
    }
}

WebSocket::Address WebSocket::getAddress()
{
    uv_os_sock_t fd;
    uv_fileno((uv_handle_t *) p, (uv_os_fd_t *) &fd);

    sockaddr_storage addr;
    socklen_t addrLength = sizeof(addr);
    getpeername(fd, (sockaddr *) &addr, &addrLength);

    static __thread char buf[INET6_ADDRSTRLEN];

    if (addr.ss_family == AF_INET) {
        sockaddr_in *ipv4 = (sockaddr_in *) &addr;
        inet_ntop(AF_INET, &ipv4->sin_addr, buf, sizeof(buf));
        return {ntohs(ipv4->sin_port), buf, "IPv4"};
    } else {
        sockaddr_in6 *ipv6 = (sockaddr_in6 *) &addr;
        inet_ntop(AF_INET6, &ipv6->sin6_addr, buf, sizeof(buf));
        return {ntohs(ipv6->sin6_port), buf, "IPv6"};
    }
}

void WebSocket::onReadable(uv_poll_t *p, int status, int events)
{
    SocketData *socketData = (SocketData *) p->data;

    // this one is not needed, read will do this!
    if (status < 0) {
        WebSocket(p).close(true, 1006);
        return;
    }

    char *src = socketData->server->recvBuffer;
    memcpy(src, socketData->spill, socketData->spillLength);
    uv_os_sock_t fd;
    uv_fileno((uv_handle_t *) p, (uv_os_fd_t *) &fd);

    // this whole SSL part should be shared with HTTPSocket
    ssize_t received;
    if (socketData->ssl) {
        received = SSL_read(socketData->ssl, src + socketData->spillLength, Server::LARGE_BUFFER_SIZE - socketData->spillLength);

        // do not treat SSL_ERROR_WANT_* as hang ups
        if (received < 1) {
            switch (SSL_get_error(socketData->ssl, received)) {
            case SSL_ERROR_WANT_WRITE:
            case SSL_ERROR_WANT_READ:
                return;
            }
        }
    } else {
        received = recv(fd, src + socketData->spillLength, Server::LARGE_BUFFER_SIZE - socketData->spillLength, 0);
    }

    if (received == SOCKET_ERROR || received == 0) {
        // do we have a close frame in our buffer, and did we already set the state as CLOSING?
        if (socketData->state == CLOSING && socketData->controlBuffer.length()) {
            std::tuple<unsigned short, char *, size_t> closeFrame = Parser::parseCloseFrame(socketData->controlBuffer);
            if (!std::get<0>(closeFrame)) {
                std::get<0>(closeFrame) = 1006;
            }
            WebSocket(p).close(true, std::get<0>(closeFrame), std::get<1>(closeFrame), std::get<2>(closeFrame));
        } else {
            WebSocket(p).close(true, 1006);
        }
        return;
    }

    // do not parse any data once in closing state
    if (socketData->state == CLOSING) {
        return;
    }

    // cork sends into one large package
#ifdef __linux
    int cork = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(int));
#endif

    Parser::consume(socketData->spillLength + received, src, socketData, p);

#ifdef __linux
    cork = 0;
    setsockopt(fd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(int));
#endif
}

void WebSocket::initPoll(Server *server, uv_os_sock_t fd, void *ssl, void *perMessageDeflate)
{
    uv_poll_init_socket(server->loop, p, fd);
    SocketData *socketData = new SocketData;
    socketData->pmd = (PerMessageDeflate *) perMessageDeflate;
    socketData->server = server;

    socketData->ssl = (SSL *) ssl;
    if (socketData->ssl) {
#ifndef NODEJS_WINDOWS
        SSL_set_fd(socketData->ssl, fd);
#endif
        SSL_set_mode(socketData->ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);
    }

    p->data = socketData;
    uv_poll_start(p, UV_READABLE, onReadable);
}

WebSocket::WebSocket(uv_poll_t *p) : p(p)
{

}

void WebSocket::link(uv_poll_t *next)
{
    SocketData *nextData = (SocketData *) next->data;
    nextData->prev = p;
    SocketData *data = (SocketData *) p->data;
    data->next = next;
}

uv_poll_t *WebSocket::next()
{
    return ((SocketData *) p->data)->next;
}

WebSocket::operator bool()
{
    return p;
}

void *WebSocket::getData()
{
    return ((SocketData *) p->data)->data;
}

void WebSocket::setData(void *data)
{
    ((SocketData *) p->data)->data = data;
}

void WebSocket::close(bool force, unsigned short code, char *data, size_t length)
{
    uv_os_sock_t fd;
    uv_fileno((uv_handle_t *) p, (uv_os_fd_t *) &fd);
    SocketData *socketData = (SocketData *) p->data;

    if (socketData->state != CLOSING) {
        socketData->state = CLOSING;
        if (socketData->prev == socketData->next) {
            socketData->server->clients = nullptr;
        } else {
            if (socketData->prev) {
                ((SocketData *) socketData->prev->data)->next = socketData->next;
            } else {
                socketData->server->clients = socketData->next;
            }
            if (socketData->next) {
                ((SocketData *) socketData->next->data)->prev = socketData->prev;
            }
        }

        // reuse prev as timer, mark no timer set
        socketData->prev = nullptr;

        // call disconnection callback on first close (graceful or force)
        socketData->server->disconnectionCallback(p, code, data, length);
    } else if (!force) {
        std::cerr << "WARNING: Already gracefully closed: " << p << std::endl;
        return;
    }

    if (force) {
        // delete all messages in queue
        while (!socketData->messageQueue.empty()) {
            SocketData::Queue::Message *message = socketData->messageQueue.front();
            if (message->callback) {
                message->callback(nullptr, message->callbackData, true);
            }
            socketData->messageQueue.pop();
        }

        uv_poll_stop(p);
        uv_close((uv_handle_t *) p, [](uv_handle_t *handle) {
            delete (uv_poll_t *) handle;
        });

        ::close(fd);
        SSL_free(socketData->ssl);
        socketData->controlBuffer.clear();

        // cancel force close timer
        if (socketData->prev) {
            uv_timer_stop((uv_timer_t *) socketData->prev);
            uv_close((uv_handle_t *) socketData->prev, [](uv_handle_t *handle) {
                delete (uv_timer_t *) handle;
            });
        }

        delete socketData->pmd;
        delete socketData;
    } else {
        // force close after 15 seconds
        socketData->prev = (uv_poll_t *) new uv_timer_t;
        uv_timer_init(socketData->server->loop, (uv_timer_t *) socketData->prev);
        ((uv_timer_t *) socketData->prev)->data = p;
        uv_timer_start((uv_timer_t *) socketData->prev, [](uv_timer_t *timer) {
            WebSocket((uv_poll_t *) timer->data).close(true, 1006);
        }, 15000, 0);

        char *sendBuffer = socketData->server->sendBuffer;
        if (code) {
            length = std::min<size_t>(1024, length) + 2;
            *((uint16_t *) &sendBuffer[length + 2]) = htons(code);
            memcpy(&sendBuffer[length + 4], data, length - 2);
        }
        write(sendBuffer, formatMessage(sendBuffer, &sendBuffer[length + 2], length, CLOSE, length, false), false, [](WebSocket webSocket, void *data, bool cancelled) {
            if (!cancelled) {
                uv_os_sock_t fd;
                uv_fileno((uv_handle_t *) webSocket.p, (uv_os_fd_t *) &fd);
                SocketData *socketData = (SocketData *) webSocket.p->data;
                if (socketData->ssl) {
                    SSL_shutdown(socketData->ssl);
                }
                shutdown(fd, SHUT_WR);
            }
        });
    }
}

// async Unix send (has a Message struct in the start if transferOwnership OR preparedMessage)
void WebSocket::write(char *data, size_t length, bool transferOwnership, void(*callback)(WebSocket webSocket, void *data, bool cancelled), void *callbackData, bool preparedMessage)
{
    uv_os_sock_t fd;
    uv_fileno((uv_handle_t *) p, (uv_os_fd_t *) &fd);

    ssize_t sent = 0;
    SocketData *socketData = (SocketData *) p->data;
    if (!socketData->messageQueue.empty()) {
        goto queueIt;
    }

    if (socketData->ssl) {
        sent = SSL_write(socketData->ssl, data, length);
    } else {
        sent = ::send(fd, data, length, MSG_NOSIGNAL);
    }

    if (sent == (int) length) {
        // everything was sent in one go!
        if (transferOwnership) {
            delete [] (data - sizeof(SocketData::Queue::Message));
        }

        if (callback) {
            callback(p, callbackData, false);
        }

    } else {
        // not everything was sent
        if (sent == SOCKET_ERROR) {
            // check to see if any error occurred
            if (socketData->ssl) {
                int error = SSL_get_error(socketData->ssl, sent);
                if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) {
                    goto queueIt;
                }
            } else {
#ifdef _WIN32
                if (WSAGetLastError() == WSAENOBUFS || WSAGetLastError() == WSAEWOULDBLOCK) {
#else
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
#endif
                    goto queueIt;
                }
            }

            // error sending!
            if (transferOwnership) {
                delete [] (data - sizeof(SocketData::Queue::Message));
            }

            if (callback) {
                callback(p, callbackData, true);
            }

            return;
        } else {

            queueIt:
            sent = std::max<ssize_t>(sent, 0);

            // queue the rest of the message!
            SocketData::Queue::Message *messagePtr;
            if (transferOwnership) {
                messagePtr = (SocketData::Queue::Message *) (data - sizeof(SocketData::Queue::Message));
                messagePtr->data = data + sent;
                messagePtr->length = length - sent;
                messagePtr->nextMessage = nullptr;
            } else if (preparedMessage) {
                // these allocations are always small and could belong to the same memory block
                // best would be to use a stack and delete the whole stack when the prepared message gets deleted
                messagePtr = (SocketData::Queue::Message *) new char[sizeof(SocketData::Queue::Message)];
                messagePtr->data = data + sent;
                messagePtr->length = length - sent;
                messagePtr->nextMessage = nullptr;
            } else {
                // we need to copy the buffer
                messagePtr = (SocketData::Queue::Message *) new char[sizeof(SocketData::Queue::Message) + length - sent];
                messagePtr->length = length - sent;
                messagePtr->data = ((char *) messagePtr) + sizeof(SocketData::Queue::Message);
                messagePtr->nextMessage = nullptr;
                memcpy(messagePtr->data, data + sent, messagePtr->length);
            }

            messagePtr->callback = callback;
            messagePtr->callbackData = callbackData;
            ((SocketData *) p->data)->messageQueue.push(messagePtr);

            // only start this if we just broke the 0 queue size!
            uv_poll_start(p, UV_WRITABLE | UV_READABLE, [](uv_poll_t *handle, int status, int events) {

                // handle all poll errors with forced disconnection
                if (status < 0) {
                    WebSocket(handle).close(true, 1006);
                    return;
                }

                // handle reads if available
                if (events & UV_READABLE) {
                    onReadable(handle, status, events);
                    if (!(events & UV_WRITABLE)) {
                        return;
                    }
                }

                SocketData *socketData = (SocketData *) handle->data;

                if (socketData->state == CLOSING) {
                    if (uv_is_closing((uv_handle_t *) handle)) {
                        return;
                    } else {
                        uv_poll_start(handle, UV_READABLE, onReadable);
                    }
                }

                uv_os_sock_t fd;
                uv_fileno((uv_handle_t *) handle, (uv_os_fd_t *) &fd);

                do {
                    SocketData::Queue::Message *messagePtr = socketData->messageQueue.front();

                    ssize_t sent;
                    if (socketData->ssl) {
                        sent = SSL_write(socketData->ssl, messagePtr->data, messagePtr->length);
                    } else {
                        sent = ::send(fd, messagePtr->data, messagePtr->length, MSG_NOSIGNAL);
                    }

                    if (sent == (int) messagePtr->length) {

                        if (messagePtr->callback) {
                            messagePtr->callback(handle, messagePtr->callbackData, false);
                        }

                        socketData->messageQueue.pop();
                    } else {
                        if (sent == SOCKET_ERROR) {
                            // check to see if any error occurred
                            if (socketData->ssl) {
                                int error = SSL_get_error(socketData->ssl, sent);
                                if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) {
                                    return;
                                }
                            } else {
                #ifdef _WIN32
                                if (WSAGetLastError() == WSAENOBUFS || WSAGetLastError() == WSAEWOULDBLOCK) {
                #else
                                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                #endif
                                    return;
                                }
                            }

                            // error sending!
                            uv_poll_start(handle, UV_READABLE, onReadable);
                            return;
                        } else {
                            // update the Message
                            messagePtr->data += sent;
                            messagePtr->length -= sent;
                            return;
                        }
                    }
                } while (!socketData->messageQueue.empty());

                // only receive when we have fully sent everything
                uv_poll_start(handle, UV_READABLE, onReadable);
            });
        }
    }
}

}
